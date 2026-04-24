#include "search.hpp"

#include "defs.hpp"
#include "move.hpp"
#include "movepick.hpp"
#include "search_utils.hpp"

#include <atomic>
#include <cassert>
#include <print>

namespace Lyra {

/******************************************\
|==========================================|
|              Search Helpers              |
|==========================================|
\******************************************/

void Worker::start(const TimeControl &tc) {
  Colour stm = board_.stm();

  if (is_main()) {
    clock_.set(stm, tc);
    tt_.incr_age();
  }

  StackEntry  stack[MaxDepth + StackOffset]{};
  StackEntry *se = stack + StackOffset;

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
  Eval alpha = -EvalInf;
  Eval beta  = EvalInf;

  Eval val = search<Us, PV>(se, alpha, beta, depth_ + 1);

  if (stop_.load(std::memory_order::relaxed)) return;

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
Eval Worker::search(StackEntry *se, Eval alpha, Eval beta, Depth depth) {
  constexpr bool pv       = NT == PV;
  const bool     in_check = board_.in_check();
  const bool     root     = ply_ == 0;

  if (depth <= 0) return qsearch<Us, NT>(se, alpha, beta);

  se->pv.clear();

  if (!root) {
    if (clock_.stop(nodes_)) return EvalStop;
    if (board_.is_draw(ply_)) return EvalDraw;
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

  Move tt_move = NoMove;

  if (tt_hit) {
    // if (!pv && tte.depth >= depth && can_use_bound(tte.bound, tte.value, beta)) {
    //   return tte.value;
    // }

    tt_move = tte.move;
  }

  /********************************\
  |        Main Search Loop        |
  \********************************/

  Eval val         = 0;
  Eval best        = -EvalInf;
  int  move_count  = 0;
  Move move        = NoMove;
  Move best_move   = NoMove;
  bool full_search = false;

  (se + 1)->killer.fill(NoMove);

  MovePicker<Us> mp{MPType::Main, board_, mostats(se), tt_move, depth};
  while ((move = mp.next())) {
    const Depth new_depth    = depth - 1;
    const U64   cached_nodes = nodes_;
    move_count++;

    do_move<Us>(se, move);

    /********************************\
    |      Late move reductions      |
    \********************************/

    if (can_lmr(depth, move_count, pv, move)) {
      Depth r = 1;

      val = -search<~Us, NonPV>(se + 1, -alpha - 1, -alpha, new_depth - r);

      full_search = val > alpha && r > 0;
    } else {
      full_search = !pv || move_count > 1;
    }

    if (full_search) val = -search<~Us, NonPV>(se + 1, -alpha - 1, -alpha, new_depth);

    if (pv && (move_count == 1 || val > alpha))
      val = -search<~Us, PV>(se + 1, -beta, -alpha, new_depth);

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
  }

  if (best_move) update_all_stats(se, depth, best_move);

  if (move_count == 0) best = in_check ? mated_in(ply_) : EvalDraw;

  tt_.write(board_.key(), depth, ply_,
            best >= beta        ? Bound::Lower
            : (pv && best_move) ? Bound::Exact
                                : Bound::Upper,
            best_move, EvalInvalid, best);

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

  if (clock_.stop(nodes_)) return EvalStop;
  if (board_.is_draw(ply_from_null_)) return EvalDraw;
  if (ply_ >= MaxDepth) return in_check ? EvalDraw : board_.eval();

  /********************************\
  |   Transposition Table Lookup   |
  \********************************/

  auto [tt_hit, tte] = tt_.read(board_.key(), ply_);

  Move tt_move = NoMove;

  if (tt_hit) {
    // if (!pv && can_use_bound(tte.bound, tte.value, beta)) {
    //   return tte.value;
    // }

    tt_move = tte.move;
  }

  /********************************\
  |            Stand Pat           |
  \********************************/

  // The current eval is the lower bound because we can just not capture anything (assume its not a
  // zugzwang) If lower bound >= beta, then we fail high (opponent has better options) If lower
  // bound > alpha, then we update alpha (the best we can do)
  Eval best = board_.eval();
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

    do_move<Us>(se, move);
    Eval val = -qsearch<~Us, PV>(se + 1, -beta, -alpha);
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
            EvalInvalid, best);

  return best;
}

} // namespace Lyra
