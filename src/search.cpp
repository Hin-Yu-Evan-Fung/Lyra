#include "search.hpp"

#include "defs.hpp"
#include "history.hpp"
#include "move.hpp"
#include "movepick.hpp"
#include "params.hpp"
#include "search_utils.hpp"
#include "utils.hpp"

#include <atomic>
#include <cassert>

namespace Lyra {

/******************************************\
|==========================================|
|              Search Helpers              |
|==========================================|
\******************************************/

void Worker::start(TimeControl tc) {
  Colour stm = board_.stm();

  if (is_main()) {
    clock_.set(stm, tc);
    tt_.incr_age();
  }

  StackEntry  stack[MaxDepth + StackOffset]{};
  StackEntry *se = stack + StackOffset;

  for (int i = 1; i <= StackOffset; i++) (se - i)->cont = &hist_cont_[wP][A1];

  while (should_search_deeper()) {

    stm == White ? aspwin<White>(se) : aspwin<Black>(se);

    if (stop_.load(std::memory_order::relaxed)) break;

    uci_report(se->pv);

    depth_ += 1;
  }

  report_best_move();
}

template <Colour Us>
void Worker::aspwin(StackEntry *se) {
  Eval  alpha  = -EvalInf;
  Eval  beta   = EvalInf;
  Eval  window = 20;
  Depth r      = 0;
  Eval  val;

  if (depth_ >= 5) {
    alpha = std::max(eval_ - window, -EvalInf);
    beta  = std::min(eval_ + window, EvalInf);
  }

  while (true) {

    val = negamax<Us, PV>(se, alpha, beta, depth_ + 1 - r, false);

    if (stop_.load(std::memory_order::relaxed)) return;

    if (val <= alpha) {
      beta  = (alpha + beta) / 2;
      alpha = std::max(val - window, -EvalInf);
      r     = 0;
    } else if (val >= beta) {
      beta = std::min(val + window, EvalInf);
      r += !is_terminal(val);
    } else {
      break;
    }

    window += window / 2;
  }

  eval_     = val;
  avg_eval_ = depth_ > 1 ? (avg_eval_ * 8 + eval_ * 2) / 10 : eval_;

  if (best_move_ != se->pv.moves[0]) {
    best_move_            = se->pv.moves[0];
    last_best_move_depth_ = depth_;
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
Eval Worker::negamax(StackEntry *se, Eval alpha, Eval beta, Depth depth, bool cutnode) {
  constexpr bool pv       = NT == PV;
  const bool     in_check = board_.in_check();
  const bool     root     = ply_ == 0;
  const bool     singular = se->excl != NoMove;

  if (depth <= 0) return qsearch<Us, NT>(se, alpha, beta);

  se->pv.clear();
  seldepth_ = std::max(seldepth_, Depth(ply_ + 1));

  if (!root) {
    if (clock_.stop(nodes_)) return EvalStop;
    if (board_.is_draw(ply_from_null_)) return EvalDraw;
    if (ply_ >= MaxDepth) return in_check ? EvalDraw : board_.eval();

    // Our guaranteed score will not be worse than mated in ply.
    alpha = std::max(alpha, mated_in(ply_));
    // Opponent's worst case scenario will not be worse than mate in ply_ + 1.
    beta = std::min(beta, mate_in(ply_ + 1));
    // if our guaranteed score is better than the opponent's guaranteed score, no need to continue
    // to search this.
    if (alpha >= beta) return alpha;
  }

  /********************************\
  |   Transposition Table Lookup   |
  \********************************/

  auto [tt_hit, tte] = tt_.read(board_.key(), ply_);

  Bound tt_bound = Bound::None;
  Depth tt_depth = 0;
  Eval  tt_eval  = EvalInvalid;
  Move  tt_move  = NoMove;
  Eval  tt_value = EvalInvalid;

  if (tt_hit) {
    if (!pv && !singular && tte.depth >= depth && can_use_bound(tte.bound, tte.value, beta)) {
      return tte.value;
    }

    tt_bound = tte.bound;
    tt_depth = tte.depth;
    tt_eval  = tte.eval;
    tt_move  = tte.move;
    tt_value = tte.value;
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

    if (is_valid(tt_value) && can_use_bound(tt_bound, tt_value, eval)) eval = tt_value;
  }

  const bool improving = !in_check && ply_ >= 2 && se->eval > (se - 2)->eval;

  /********************************\
  |         Forward Pruning        |
  \********************************/

  if (!pv && !in_check && !singular) {

    /********************************\
    |    Reverse Futility Pruning    |
    \********************************/

    // If the eval of the current position is way above beta, then we can not do anything and still
    // be good enough

    if (can_rfp(depth, eval, beta)) return eval;

    /********************************\
    |        Null Move Pruning       |
    \********************************/

    // If the position seems to be quite good, then we give the opponent a second move, and if that
    // doesn't help the opponent then we can prune this node

    if (can_nmp(se, depth, eval, beta)) {
      Depth r = nmp_reduction(depth);

      do_null_move<Us>(se);
      Eval val = -negamax<~Us, NonPV>(se + 1, -beta, -beta + 1, depth - r, false);
      undo_null_move<Us>(se);

      if (val >= beta) return is_win(val) ? beta : val;
    }
  }

  /********************************\
  |  Internal Iterative Reductions |
  \********************************/

  if ((pv || cutnode) && depth >= 4 && !tt_move) depth--;

  /********************************\
  |        Main Search Loop        |
  \********************************/

  Eval val         = 0;
  Eval best        = -EvalInf;
  int  move_count  = 0;
  Move move        = NoMove;
  Move best_move   = NoMove;
  bool full_search = false;

  std::vector<Move> captures, quiets;

  (se + 1)->killer.fill(NoMove);

  MovePicker<Us> mp{MPType::Main, board_, mostats(se), tt_move, depth};
  while ((move = mp.next())) {
    const bool    is_cap       = MoveUtils::is_capture(move);
    const U64     cached_nodes = nodes_;
    const PieceTo p            = piece_to(board_, move);

    if (move == se->excl) continue;

    move_count++;

    Eval  hist      = is_cap ? 0 : hist_quiet_[p.pc][p.to];
    Depth new_depth = depth - 1 + in_check;
    Depth r         = lmr_reduction(depth, move_count);

    /********************************\
    |             Pruning            |
    \********************************/

    if (!pv && !in_check && !mp.skip_quiet_ && !is_terminal(best)) {

      /********************************\
      |        Late Move Pruning       |
      \********************************/

      // We can trust move ordering at the leaf nodes and prune late quiet moves
      if (can_lmp(depth, move_count, improving)) {
        mp.skip_quiet_ = true;
      }

      /********************************\
      |        Futility Pruning        |
      \********************************/

      // At low depths, we can skip quiets if its unlikely that a positional move can give us enough
      // advantage
      if (can_fp(new_depth - r, eval, alpha)) {
        mp.skip_quiet_ = true;
      }
    }

    if (mp.stage() > GOOD_CAP && can_see(depth, move, best)) continue;

    /********************************\
    |       Singular Extensions      |
    \********************************/

    Depth ext = 0;
    if (!root && !singular && can_singular(tt_bound, tt_depth, tt_move, tt_value, depth, move)) {
      Eval s_beta = std::max(tt_value - 2 * depth, -EvalMate);

      se->excl = move;
      Eval val = negamax<Us, NonPV>(se, s_beta - 1, s_beta, (depth - 1) / 2, cutnode);
      se->excl = NoMove;

      if (val < s_beta) ext = 1;
    }

    new_depth += ext;

    do_move<Us>(se, move);

    /********************************\
    |      Late move reductions      |
    \********************************/

    if (can_lmr(depth, move_count, pv, move)) {

      r -= hist / LmrMultHist;

      Depth d = std::clamp(new_depth - r, 1, (int)new_depth);
      val     = -negamax<~Us, NonPV>(se + 1, -alpha - 1, -alpha, d, true);

      full_search = val > alpha && new_depth > d;
    } else {
      full_search = !pv || move_count > 1;
    }

    /********************************\
    |   Principal Variation Search   |
    \********************************/

    if (full_search) val = -negamax<~Us, NonPV>(se + 1, -alpha - 1, -alpha, new_depth, !cutnode);

    if (pv && (move_count == 1 || val > alpha))
      val = -negamax<~Us, NT>(se + 1, -beta, -alpha, new_depth, false);

    undo_move<Us>(se);

    // If we are stopping, return a placeholder score.
    if (stop_.load(std::memory_order::relaxed)) return EvalStop;

    if (root) clock_.update_effort(nodes_ - cached_nodes, move);

    /********************************\
    |       Alpha Beta Pruning       |
    \********************************/

    // If val > alpha, update pv and alpha.
    // If val >= beta (Fail High), this is too good to be played, prune this branch.

    if (val > best) {
      best = val;
      if (val > alpha) {
        best_move = move;
        if (pv) se->pv.update((se + 1)->pv, move);
        if (val >= beta) break;
        alpha = val;
      }
    }

    if (move != best_move) (is_cap ? captures : quiets).push_back(move);
  }

  if (best_move) update_all_stats(se, depth, best_move, captures, quiets);

  if (move_count == 0) best = singular ? alpha : in_check ? mated_in(ply_) : EvalDraw;

  if (!singular)
    tt_.write(board_.key(), depth, ply_,
              best >= beta        ? Bound::Lower
              : (pv && best_move) ? Bound::Exact
                                  : Bound::Upper,
              best_move, raw_eval, best);

  return best;
}

// ** Quiescence Search **
// Alpha is our guaranteed score
// Beta is our opponent's guaranteed score
template <Colour Us, Worker::NodeType NT>
Eval Worker::qsearch(StackEntry *se, Eval alpha, Eval beta) {
  constexpr bool pv       = NT == PV;
  const bool     in_check = board_.in_check();

  se->pv.clear();
  seldepth_ = std::max(seldepth_, Depth(ply_ + 1));

  if (clock_.stop(nodes_)) return EvalStop;
  if (board_.is_draw(ply_from_null_)) return EvalDraw;
  if (ply_ >= MaxDepth) return in_check ? EvalDraw : board_.eval();

  /********************************\
  |   Transposition Table Lookup   |
  \********************************/

  auto [tt_hit, tte] = tt_.read(board_.key(), ply_);

  Bound tt_bound = Bound::None;
  Eval  tt_eval  = EvalInvalid;
  Move  tt_move  = NoMove;
  Eval  tt_value = EvalInvalid;

  if (tt_hit) {
    if (!pv && can_use_bound(tte.bound, tte.value, beta)) {
      return tte.value;
    }

    tt_bound = tte.bound;
    tt_eval  = tte.eval;
    tt_move  = tte.move;
    tt_value = tte.value;
  }

  /********************************\
  |           Static Eval          |
  \********************************/

  Eval raw_eval = -EvalInf;
  Eval best     = -EvalInf;

  best = se->eval = raw_eval = is_valid(tt_eval) ? tt_eval : board_.eval();

  if (is_valid(tt_value) && can_use_bound(tt_bound, tt_value, best)) best = tt_value;

  /********************************\
  |            Stand Pat           |
  \********************************/

  // The current eval is the lower bound because we can just not capture anything (assume its not a
  // zugzwang) If lower bound >= beta, then we fail high (opponent has better options) If lower
  // bound > alpha, then we update alpha (the best we can do)
  if (best >= beta) return best;
  alpha = std::max(alpha, best);

  /********************************\
  |        Main Search Loop        |
  \********************************/

  Move move      = NoMove;
  Move best_move = NoMove;

  (se + 1)->killer.fill(NoMove);

  MovePicker<Us> mp{MPType::QSearch, board_, mostats(se), tt_move, DepthQS};
  while ((move = mp.next())) {

    if (!is_loss(best)) {

      /********************************\
      |           SEE Pruning          |
      \********************************/

      // Can safely (probably!) ignore losing captures.
      if (!board_.see(move, -30)) continue;
    }

    do_move<Us>(se, move);
    Eval val = -qsearch<~Us, NT>(se + 1, -beta, -alpha);
    undo_move<Us>(se);

    /********************************\
    |       Alpha Beta Pruning       |
    \********************************/

    // If val > alpha, update pv and alpha.
    // If val >= beta (Fail High), this is too good to be played, prune this branch.

    if (val > best) {
      best = val;
      if (val > alpha) {
        best_move = move;
        if (pv) se->pv.update((se + 1)->pv, move);
        if (val >= beta) break;
        alpha = val;
      }
    }
  }

  tt_.write(board_.key(), DepthQS, ply_, best >= beta ? Bound::Lower : Bound::Upper, best_move,
            raw_eval, best);

  return best;
}

} // namespace Lyra
