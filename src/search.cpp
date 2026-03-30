#include "search.hpp"

#include "defs.hpp"
#include "history.hpp"
#include "move.hpp"
#include "movepick.hpp"
#include "search_utils.hpp"
#include "tt.hpp"
#include "utils.hpp"

#include <atomic>

namespace Lyra {

using namespace MoveUtils;

void Worker::start(TimeControl tc) {
  Colour stm = board_.stm();

  if (is_main()) {
    clock_.set(stm, tc);
    tt_.incr_age();
  }

  StackEntry  stack[MaxDepth + StackOffset]{};
  StackEntry *se = stack + StackOffset;

  for (int i = 0; i <= StackOffset; i++) (se - i)->cont = &cont_table_[wP][A1];
  for (int i = 0; i < MaxDepth; i++) (se + i)->ply = i;

  while (
      depth_ < MaxDepth
      && !clock_.stop_iter(depth_, last_best_move_depth_, avg_eval_, eval_, nodes_, best_move_)) {

    if (stm == White)
      aspwin<White>(se);
    else
      aspwin<Black>(se);

    if (stop_.load(std::memory_order::relaxed)) break;

    uci_report(se->pv);

    if (best_move_ != se->pv.moves[0]) {
      best_move_            = se->pv.moves[0];
      last_best_move_depth_ = depth_;
    }

    depth_ += 1;
  }

  report_best_move();
}

// TODO: Aspiration windows
template <Colour Us>
void Worker::aspwin(StackEntry *se) {
  Eval alpha = -EvalInf;
  Eval beta  = EvalInf;

  eval_     = negamax<Us, PV>(se, alpha, beta, depth_ + 1);
  avg_eval_ = depth_ > 1 ? (avg_eval_ * 8 + eval_ * 2) / 10 : eval_;
}

/******************************************\
|==========================================|
|               Main Search                |
|==========================================|
\******************************************/

// ** Search **
// Alpha is our guaranteed score
// Beta is our opponent's guaranteed score
template <Colour Us, Worker::NodeType NT>
Eval Worker::negamax(StackEntry *se, Eval alpha, Eval beta, Depth depth) {
  constexpr bool pv       = NT == PV;
  const bool     root     = se->ply == 0;
  const bool     in_check = board_.in_check();

  if (depth <= 0) return qsearch<Us, NT>(se, alpha, beta);

  se->pv.clear();
  seldepth_ = std::max(seldepth_, Depth(se->ply + 1));

  /********************************\
  |      Mate Distance Pruning     |
  \********************************/

  if (!root) {
    if (clock_.stop(nodes_)) return EvalStop;
    if (board_.is_draw(se->ply)) return EvalDraw;
    if (se->ply >= MaxDepth - 1) return in_check ? EvalDraw : board_.eval();

    // Our guaranteed score will not be worse than mated in ply.
    alpha = std::max(alpha, mated_in(se->ply));
    // Opponent's worst case scenario will not be worse than mate in ply_ + 1.
    beta = std::min(beta, mate_in(se->ply + 1));
    // if our guaranteed score is better than the opponent's guaranteed score,
    // no need to continue to search this.
    if (alpha >= beta) return alpha;
  }

  /********************************\
  |   Transposition Table Lookup   |
  \********************************/

  auto [tt_hit, tt_entry] = tt_.read(board_.key(), se->ply);

  Move tt_move = NoMove;

  if (tt_hit) {
    if (!pv && tt_entry.depth >= depth && can_tt_cutoff(tt_entry, alpha, beta)) {
      return tt_entry.value;
    }

    tt_move = tt_entry.move;
  }

  Eval eval = board_.eval();

  if (!pv && !in_check) {
    if (depth >= 2 && (se - 1)->move != NullMove && eval >= beta && !is_win(eval) && !is_loss(beta)
        && board_.has_non_pawn_material(board_.stm())) {

      Depth r = 2;

      do_null_move<Us>(se);
      Eval val = -negamax<~Us, NonPV>(se + 1, -beta, -beta + 1, depth - r);
      undo_null_move<Us>(se);

      if (val >= beta) return is_win(val) ? beta : val;
    }
  }

  /********************************\
  |        Main Search Loop        |
  \********************************/

  // Clear child killer moves
  (se + 1)->killer.fill(NoMove);

  Eval best        = -EvalInf;
  Eval val         = EvalInvalid;
  int  move_count  = 0;
  Move move        = NoMove;
  Move best_move   = NoMove;
  bool full_search = false;

  std::vector<Move> captures, quiets;

  MovePicker<Us> mp{MPType::Main, board_, mostats(se), tt_move, depth};

  while ((move = mp.next())) {
    const Depth new_depth = depth - 1;
    const bool  is_cap    = is_capture(move);

    (is_cap ? captures : quiets).push_back(move);

    move_count++;

    if (!pv && !in_check && board_.has_non_pawn_material(board_.stm())) {

      /********************************\
      |        Late Move Pruning       |
      \********************************/
      // Near leaf nodes, we can safely (hopefully!) prune quiet moves that are ranked low in move
      // ordering
      if (move_count >= 3 + depth * depth) mp.skip_quiet();

      /********************************\
      |          SEE Pruning           |
      \********************************/

      // Near leaf nodes, we can safely (hopefully!) prune moves that lose in terms of exchanges
      if (mp.stage() > GOOD_CAP && !is_terminal(best) && depth <= 10
          && !board_.see(move, is_capture(move) ? -70 * depth : -20 * depth * depth))
        continue;
    }

    do_move<Us>(se, move);

    /********************************\
    |       Late move reduction      |
    \********************************/

    U64 cached_nodes = nodes_;

    // 1. Assume the first move is the best move.
    // 2. Use a null window with reduced search to prove that later moves are worse.
    if (depth > 2 && move_count > 2 + pv && !is_promo(move) && !is_capture(move)) {
      Depth r = 1;

      Depth d     = std::clamp(new_depth - r, 1, new_depth + 1);
      val         = -negamax<~Us, NonPV>(se + 1, -alpha - 1, -alpha, d);
      full_search = val > alpha && new_depth > d;
    } else {
      full_search = !pv || move_count > 1;
    }

    /********************************\
    |   Principal Variation Search   |
    \********************************/

    // 3. If reduced search showed that the move could be good, search it at full depth.
    if (full_search) val = -negamax<~Us, NonPV>(se + 1, -alpha - 1, -alpha, new_depth);

    // 4. If its the first move, or the later move is promising, then do a full window search
    if (pv && (move_count == 1 || val > alpha))
      val = -negamax<~Us, NT>(se + 1, -beta, -alpha, new_depth);

    undo_move<Us>(se);

    // If we are stopping, return a placeholder score.
    if (stop_.load(std::memory_order::relaxed)) return EvalStop;

    if (root) {
      clock_.update_effort(nodes_ - cached_nodes, move);
    }

    /********************************\
    |       Alpha Beta Pruning       |
    \********************************/

    // If val > alpha, update pv and alpha.
    // If val >= beta (Fail High), this is too good to be played, prune this branch.

    if (val > best) {
      best = val;
      if (val > alpha) {
        best_move = move;
        if (pv) se->pv.update((se + 1)->pv, best_move);
        if (val >= beta) break;
        alpha = val;
      }
    }
  }

  /********************************\
  |         Update History         |
  \********************************/

  if (best_move) update_all_stats(se, depth, best_move, captures, quiets);

  /********************************\
  |        Draw / mate score       |
  \********************************/

  if (move_count == 0) best = board_.in_check() ? mated_in(se->ply) : EvalDraw;

  /********************************\
  |   Transposition table write    |
  \********************************/

  // If we fail high, we have a lower bound for how good this pos is.
  // If we are in PV and we have a best move, then we have an exact bound.
  tt_.write(board_.key(), depth, se->ply,
            best >= beta        ? TTBound::Lower
            : (pv && best_move) ? TTBound::Exact
                                : TTBound::Upper,
            best_move, 0, best);

  return best;
}

// ** Quiescence Search **
// Alpha is our guaranteed score
// Beta is our opponent's guaranteed score
template <Colour Us, Worker::NodeType NT>
Eval Worker::qsearch(StackEntry *se, Eval alpha, Eval beta) {
  constexpr bool pv = NT == PV;

  se->pv.clear();
  seldepth_ = std::max(seldepth_, Depth(se->ply + 1));

  if (clock_.stop(nodes_)) return EvalStop;
  if (board_.is_draw(se->ply)) return EvalDraw;
  if (se->ply >= MaxDepth - 1) return board_.in_check() ? EvalDraw : board_.eval();

  /********************************\
  |   Transposition Table Lookup   |
  \********************************/

  auto [tt_hit, tt_entry] = tt_.read(board_.key(), se->ply);

  Move tt_move = NoMove;

  if (tt_hit) {
    if (!pv && can_tt_cutoff(tt_entry, alpha, beta)) {
      return tt_entry.value;
    }

    tt_move = tt_entry.move;
  }

  /********************************\
  |            Stand pat           |
  \********************************/

  // The current eval is the lower bound because we can just not capture
  // anything (assume its not a zugzwang) If lower bound >= beta, then we fail
  // high (opponent has better options) If lower bound > alpha, then we update
  // alpha (the best we can do)
  Eval best = board_.eval();
  if (best >= beta) return best;
  alpha = std::max(alpha, best);

  /********************************\
  |        Main Qsearch Loop       |
  \********************************/

  Move move      = NoMove;
  Move best_move = NoMove;

  // Clear killer moves
  (se + 1)->killer.fill(NoMove);
  MovePicker<Us> mp{MPType::QSearch, board_, mostats(se), tt_move, DepthQS};

  while ((move = mp.next())) {

    do_move<Us>(se, move);
    Eval val = -qsearch<~Us, PV>(se + 1, -beta, -alpha);
    undo_move<Us>(se);

    // If we are stopping, return a placeholder score
    if (stop_.load(std::memory_order::relaxed)) return EvalStop;

    /********************************\
    |       Alpha Beta Pruning       |
    \********************************/

    // If val > alpha, update pv and alpha.
    // If val >= beta (Fail High), this is too good to be played, prune this branch.

    if (val > best) {
      best = val;
      if (val > alpha) {
        best_move = move;
        if (pv) se->pv.update((se + 1)->pv, best_move);
        if (val >= beta) break;
        alpha = val;
      }
    }
  }

  /********************************\
  |   Transposition table write    |
  \********************************/

  // If we fail high, we have a lower bound for how good this pos is.
  tt_.write(board_.key(), DepthQS, se->ply, best >= beta ? TTBound::Lower : TTBound::Upper,
            best_move, 0, best);

  return best;
}

} // namespace Lyra
