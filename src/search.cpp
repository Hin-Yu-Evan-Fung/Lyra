#include "search.hpp"

#include "defs.hpp"
#include "history.hpp"
#include "move.hpp"
#include "movepick.hpp"
#include "params.hpp"
#include "search_utils.hpp"
#include "tt.hpp"
#include "utils.hpp"

#include <atomic>
#include <print>

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

template <Colour Us>
void Worker::aspwin(StackEntry *se) {
  Eval  alpha  = -EvalInf;
  Eval  beta   = EvalInf;
  Eval  window = 25;
  Depth r      = 0;

  if (depth_ >= 5) {
    alpha = std::max(eval_ - window, -EvalInf);
    beta  = std::min(eval_ + window, EvalInf);
  }

  while (true) {
    Depth r_depth = depth_ + 1 - r;
    Eval  val     = negamax<Us, PV>(se, alpha, beta, r_depth);

    if (stop_.load(std::memory_order_relaxed)) return;

    if (val <= alpha) {
      beta  = (alpha + beta) / 2;
      alpha = std::max(val - window, -EvalInf);
    } else if (val >= beta) {
      beta = std::min(val + window, EvalInf);
      if (r_depth > 1 && !is_terminal(val)) r += 1;
    } else {
      eval_     = val;
      avg_eval_ = depth_ > 1 ? (avg_eval_ * 8 + eval_ * 2) / 10 : eval_;
      break;
    }

    window += window / 2;
  }
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
  const bool     root     = ply_ == 0;
  const bool     in_check = board_.in_check();
  const bool     singular = se->excl != NoMove;

  if (depth <= 0) return qsearch<Us, NT>(se, alpha, beta);

  se->pv.clear();
  seldepth_ = std::max(seldepth_, Depth(ply_ + 1));

  /********************************\
  |      Mate Distance Pruning     |
  \********************************/

  if (!root) {
    if (clock_.stop(nodes_)) return EvalStop;
    if (board_.is_draw(ply_from_null_)) return EvalDraw;
    if (ply_ >= MaxDepth - 1) return in_check ? EvalDraw : board_.eval();

    // Our guaranteed score will not be worse than mated in ply.
    alpha = std::max(alpha, mated_in(ply_));
    // Opponent's worst case scenario will not be worse than mate in ply_ + 1.
    beta = std::min(beta, mate_in(ply_ + 1));
    // if our guaranteed score is better than the opponent's guaranteed score,
    // no need to continue to search this.
    if (alpha >= beta) return alpha;
  }

  /********************************\
  |   Transposition Table Lookup   |
  \********************************/

  auto [tt_hit, tt_entry] = tt_.read(board_.key(), ply_);

  Eval tt_eval  = EvalInvalid;
  Move tt_move  = NoMove;
  Eval tt_value = EvalInvalid;

  if (tt_hit) {
    if (!pv && !singular && tt_entry.depth >= depth && can_tt_cutoff(tt_entry, alpha, beta)) {
      return tt_entry.value;
    }

    tt_eval  = tt_entry.eval;
    tt_move  = tt_entry.move;
    tt_value = tt_entry.value;
  }

  /********************************\
  |          Static Eval           |
  \********************************/

  Eval eval     = -EvalInf;
  Eval raw_eval = -EvalInf;

  if (in_check) {
    eval = se->eval = -EvalInf;
  } else if (singular) {
    eval = se->eval;
  } else {
    eval = se->eval = raw_eval = is_valid(tt_eval) ? tt_eval : board_.eval();

    if (is_valid(tt_value) && can_use_tt_value(tt_entry, eval)) eval = tt_value;
  }

  /********************************\
  |         Forward Pruning        |
  \********************************/

  if (!pv && !in_check && !singular) {

    /********************************\
    |    Reverse Futility Pruning    |
    \********************************/

    if (can_rfp(depth, eval, beta)) return eval;

    /********************************\
    |        Null Move Pruning       |
    \********************************/

    if (can_nmp(se, depth, eval, beta)) {
      Depth r = nmp_reduction(depth);

      do_null_move<Us>(se);
      Eval val = -negamax<~Us, NonPV>(se + 1, -beta, -beta + 1, depth - r);
      undo_null_move<Us>(se);

      if (val >= beta) return is_win(val) ? beta : val;
    }
  }

  /********************************\
  |  Internal Iterative Reduction  |
  \********************************/

  if (pv && depth >= 6 && !tt_move) depth--;

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
    const bool is_cap    = is_capture(move);
    Depth      new_depth = depth - 1;

    if (move == se->excl) continue;

    (is_cap ? captures : quiets).push_back(move);

    move_count++;

    if (!pv && !in_check && board_.has_non_pawn_material(board_.stm())) {

      /********************************\
      |        Late Move Pruning       |
      \********************************/
      // Near leaf nodes, we can safely (hopefully!) prune quiet moves that are ranked low in move
      // ordering
      if (can_lmp(depth, move_count)) mp.skip_quiet();

      /********************************\
      |          SEE Pruning           |
      \********************************/

      // Near leaf nodes, we can safely (hopefully!) prune moves that lose in terms of exchanges
      if (mp.stage() > GOOD_CAP && can_see_prune(depth, move, best)) continue;
    }

    Depth ext = 0;
    if (!root && !singular && can_singular(tt_entry, depth, move)) {
      Eval s_beta = std::max(tt_value - 2 * depth, -EvalMate);

      se->excl = move;
      Eval val = negamax<Us, NonPV>(se, s_beta - 1, s_beta, (depth - 1) / 2);
      se->excl = NoMove;

      if (val < s_beta) ext = 1;
    }

    new_depth += ext ? ext : in_check;

    do_move<Us>(se, move);

    /********************************\
    |       Late move reduction      |
    \********************************/

    U64 cached_nodes = nodes_;

    // 1. Assume the first move is the best move.
    // 2. Use a null window with reduced search to prove that later moves are worse.
    if (can_lmr(depth, move_count, pv)) {
      Depth r = lmr_reduction(depth, move_count, is_cap);

      Depth d = std::clamp(new_depth - r, 1, (int)new_depth);
      val     = -negamax<~Us, NonPV>(se + 1, -alpha - 1, -alpha, d);

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

  if (move_count == 0) best = singular ? alpha : board_.in_check() ? mated_in(ply_) : EvalDraw;

  /********************************\
  |   Transposition table write    |
  \********************************/

  // If we fail high, we have a lower bound for how good this pos is.
  // If we are in PV and we have a best move, then we have an exact bound.
  if (!singular)
    tt_.write(board_.key(), depth, ply_,
              best >= beta        ? TTBound::Lower
              : (pv && best_move) ? TTBound::Exact
                                  : TTBound::Upper,
              best_move, eval, best);

  return best;
}

// ** Quiescence Search **
// Alpha is our guaranteed score
// Beta is our opponent's guaranteed score
template <Colour Us, Worker::NodeType NT>
Eval Worker::qsearch(StackEntry *se, Eval alpha, Eval beta) {
  constexpr bool pv = NT == PV;

  se->pv.clear();
  seldepth_ = std::max(seldepth_, Depth(ply_ + 1));

  if (clock_.stop(nodes_)) return EvalStop;
  if (board_.is_draw(ply_from_null_)) return EvalDraw;
  if (ply_ >= MaxDepth - 1) return board_.in_check() ? EvalDraw : board_.eval();

  /********************************\
  |   Transposition Table Lookup   |
  \********************************/

  auto [tt_hit, tt_entry] = tt_.read(board_.key(), ply_);

  Eval tt_eval  = EvalInvalid;
  Move tt_move  = NoMove;
  Eval tt_value = EvalInvalid;

  if (tt_hit) {
    if (!pv && can_tt_cutoff(tt_entry, alpha, beta)) {
      return tt_entry.value;
    }

    tt_eval  = tt_entry.eval;
    tt_move  = tt_entry.move;
    tt_value = tt_entry.value;
  }

  Eval raw_eval = -EvalInf;
  Eval best     = -EvalInf;

  best = se->eval = raw_eval = is_valid(tt_eval) ? tt_eval : board_.eval();

  if (is_valid(tt_value) && can_use_tt_value(tt_entry, best)) best = tt_value;

  /********************************\
  |            Stand pat           |
  \********************************/

  // The current eval is the lower bound because we can just not capture
  // anything (assume its not a zugzwang) If lower bound >= beta, then we fail
  // high (opponent has better options) If lower bound > alpha, then we update
  // alpha (the best we can do)

  best = raw_eval;
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
  tt_.write(board_.key(), DepthQS, ply_, best >= beta ? TTBound::Lower : TTBound::Upper, best_move,
            raw_eval, best);

  return best;
}

} // namespace Lyra
