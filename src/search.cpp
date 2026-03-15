#include "search.hpp"

#include "defs.hpp"
#include "history.hpp"
#include "movepick.hpp"
#include "tt.hpp"
#include "utils.hpp"

#include <atomic>
#include <cstdio>
#include <print>
#include <stdexcept>

namespace Lyra {

void Worker::start(TimeControl tc) {
  Colour stm = board_.stm();

  if (is_main()) {
    clock_.set(stm, tc);
    tt_.incr_age();
  }

  while (depth_ < MaxDepth && !clock_.stop_iter(depth_)) {
    if (stm == White)
      aspwin<White>();
    else
      aspwin<Black>();

    if (stop_.load(std::memory_order::relaxed)) break;

    depth_ += 1;
  }

  report_best_move();
}

// TODO: Aspiration windows
template <Colour Us>
void Worker::aspwin() {
  Eval alpha = -EvalInf;
  Eval beta  = EvalInf;

  StackEntry  stack[MaxDepth + StackOffset]{};
  StackEntry *ss = stack + StackOffset;

  for (int i = 0; i < MaxDepth; i++) (ss + i)->ply = i;

  eval_ = search<Us, PV>(ss, alpha, beta, depth_ + 1);

  if (stop_.load(std::memory_order::relaxed)) return;

  uci_report(ss->pv);
  best_move_ = ss->pv.moves[0];
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
  constexpr bool pv   = NT == PV;
  const bool     root = se->ply == 0;

  if (depth == 0) return qsearch<Us, PV>(se, alpha, beta);

  se->pv.clear();
  seldepth_ = std::max(seldepth_, Depth(se->ply + 1));

  /********************************\
  |      Mate Distance Pruning     |
  \********************************/

  if (!root) {
    if (clock_.stop(nodes_)) return EvalStop;
    if (board_.is_draw(se->ply)) return EvalDraw;
    if (se->ply >= MaxDepth - 1) return board_.in_check() ? EvalDraw : board_.eval();

    // Our guaranteed score will not be worse than mated in ply.
    alpha = std::max(alpha, EvalUtils::mated_in(se->ply));
    // Opponent's worst case scenario will not be worse than mate in ply_ + 1.
    beta = std::min(beta, EvalUtils::mate_in(se->ply + 1));
    // if our guaranteed score is better than the opponent's guaranteed score,
    // no need to continue to search this.
    if (alpha >= beta) return alpha;
  }

  /********************************\
  |   Transposition Table Lookup   |
  \********************************/

  auto [tt_hit, tt_entry] = tt_.probe(board_.key());

  Move tt_move = NoMove;

  if (tt_hit) {
    TTEntry entry = tt_entry.read(se->ply);
    tt_move       = entry.move;
  }

  /********************************\
  |        Main Search Loop        |
  \********************************/

  Eval best       = -EvalInf;
  int  move_count = 0;
  Move move       = NoMove;
  Move best_move  = NoMove;

  // Clear killer moves
  (se + 1)->killer.fill(NoMove);
  MovePicker<Us> mp{MPType::Main, board_, mostats(se), tt_move, depth};

  while ((move = mp.next())) {
    move_count++;

    nodes_++;

    board_.do_move<Us>(move);
    Eval val = -search<~Us, PV>(se + 1, -beta, -alpha, depth - 1);
    board_.undo_move<Us>();

    // If we are stopping, return a placeholder score.
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
  |         Update History         |
  \********************************/

  if (best_move && !MoveUtils::is_capture(best_move)) {
    const PieceFromTo &p = piece_from_to(board_, best_move);
    update_killer(se->killer, best_move);
    update_hist(history_[p.pc][p.to], 300 * depth - 250);
  }

  /********************************\
  |        Draw / mate score       |
  \********************************/

  if (move_count == 0) best = board_.in_check() ? EvalUtils::mated_in(se->ply) : EvalDraw;

  /********************************\
  |   Transposition table write    |
  \********************************/

  // If we fail high, we have a lower bound for how good this pos is.
  // If we are in PV and we have a best move, then we have an exact bound.
  tt_entry.write(board_.key(), tt_.age(), depth, se->ply,
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

  auto [tt_hit, tt_entry] = tt_.probe(board_.key());

  Move tt_move = NoMove;

  if (tt_hit) {
    TTEntry entry = tt_entry.read(se->ply);
    tt_move       = entry.move;
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

    nodes_++;

    board_.do_move<Us>(move);
    Eval val = -qsearch<~Us, PV>(se + 1, -beta, -alpha);
    board_.undo_move<Us>();

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
  tt_entry.write(board_.key(), tt_.age(), DepthQS, se->ply,
                 best >= beta ? TTBound::Lower : TTBound::Upper, best_move, 0, best);

  return best;
}

} // namespace Lyra
