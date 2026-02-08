#include "search/search.hpp"

#include "core/defs.hpp"
#include "core/move.hpp"
#include "search/movepick.hpp"
#include "search/utils.hpp"
#include "utils/tt.hpp"
#include "utils/utils.hpp"

#include <print>

namespace Lyra {

/******************************************\
|==========================================|
|             Search Functions             |
|==========================================|
\******************************************/

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

  std::println("bestmove {}", MoveUtils::format(best_move_, board_.chess960));
  std::println("TT read/hit %: {}%", (float)tt_hits * 100 / (float)tt_reads);
  std::println("First move fail high %: {}%", (float)fail_high_first * 100 / (float)fail_high);

  std::fflush(stdout);
}

// TODO: Aspiration windows
template <Colour Us> void Worker::aspwin() {
  Eval alpha = -EvalInf;
  Eval beta = EvalInf;

  StackEntry stack[MaxDepth + StackOffset]{};
  StackEntry *ss = stack;

  for (int i = 0; i < MaxDepth + StackOffset; i++) (ss + i)->ply = i;

  eval_ = negamax<Us, PV>(ss, alpha, beta, depth_ + 1);

  if (stop_.load(std::memory_order::relaxed)) return;

  uci_report(ss->pv);
  best_move_ = ss->pv.moves[0];
}

/******************************************\
|==========================================|
|               Main Search                |
|==========================================|
\******************************************/

// A null window search is used to prove that a move is not better than the upper score.
template <Colour Us> Eval Worker::nw_search(StackEntry *se, Eval upper, Depth depth) {
  return -negamax<~Us, NonPV>(se + 1, -(upper + 1), -upper, depth);
}

// Alpha = our guaranteed score from previous parts of the search
// Beta = opp's guaranteed score from previos parts of the search
template <Colour Us, Worker::NodeType NT> Eval Worker::negamax(StackEntry *se, Eval alpha, Eval beta, Depth depth) {
  constexpr bool pv = NT == PV;

  if (depth == 0) return qsearch<Us, NT>(se, alpha, beta);

  /********************************\
  |         Initialisation         |
  \********************************/

  se->pv.clear();
  se->in_check = board_.in_check();
  seldepth_ = std::max(seldepth_, Depth(se->ply + 1));

  /********************************\
  |    Draw check / Mate Pruning   |
  \********************************/

  // Alpha will not be worse than mated in ply
  // Beta will not be better than mating in ply + 1

  if (se->ply) {
    if (clock_.stop(nodes_)) return EvalStop;
    if (board_.is_draw(se->ply)) return EvalDraw;
    if (se->ply >= MaxDepth) return se->in_check ? EvalDraw : board_.eval();

    alpha = std::max(alpha, EvalUtils::mated_in(se->ply));
    beta = std::min(beta, EvalUtils::mate_in(se->ply + 1));
    if (alpha >= beta) return alpha;
  }

  /********************************\
  |            TT Lookup           |
  \********************************/

  auto [tt_hit, tt_entry] = tt_.probe(board_.key());
  if (tt_entry.key != 0) tt_reads++;

  Move tt_move = NoMove;

  if (tt_hit) {
    tt_hits++;
    TTEntry e = tt_entry.read(se->ply);
    tt_move = e.move;

    if (!pv && e.depth >= depth && can_tt_cutoff(e, alpha, beta)) return e.value;
  }

  /********************************\
  |             Pruning            |
  \********************************/
  Eval val = EvalDraw;
  Eval eval = board_.eval();

  if (!pv && !se->in_check) {
    /********************************\
    |        Null Move Pruning       |
    \********************************/

    // Prune this node if the following applies:
    // 1. It is safe to do null move pruning (not zugzwang, etc ...)
    // 2. Static eval indicates the move is going to fail high.
    // 3. We prove that it will fail high even if we do nothing (null move) using a reduced search.

    if (can_nmp<Us>(se, depth, eval, beta)) {
      Depth r = nmp_reduction(depth);

      do_null_move<Us>(se);
      val = nw_search<Us>(se, beta, depth - r);
      undo_null_move<Us>(se);

      if (val >= beta) return beta;
    }
  }

  /********************************\
  |           Main Search          |
  \********************************/

  Eval best = -EvalInf;
  bool full_search;
  int move_count = 0;
  Move move = NoMove;
  Move best_move = NoMove;

  MoveBuf captures(32), quiets(32);

  // Clear killer moves
  (se + 1)->killer.clear();
  MovePicker<Us> mp{board_, se->killer, stats_, tt_move, depth};

  while ((move = mp.next())) {
    move_count++;

    const bool is_cap = MoveUtils::is_capture(move);
    const bool is_good_cap = mp.stage() == GOOD_CAP;
    const Depth new_depth = depth - 1;

    do_move<Us>(se, move);

    /********************************\
    |       Late Move Reduction      |
    \********************************/

    // Assume the first move is the best move.
    // Use a null window with reduced search to prove that later moves are worse.
    if (depth >= 3 && move_count > 4 + pv && !is_good_cap) {
      Depth r = lmr_reduction(depth, move_count);
      // Reduce less for killers
      r -= mp.stage() < INIT_QUIET;
      // Reduce less for checks
      r -= board_.in_check();

      r = std::max(r, Depth(1));

      val = nw_search<Us>(se, alpha, new_depth - r);

      // If the later moves could be better, research it with full depth.
      full_search = val > alpha && r > 1;
    } else
      full_search = !pv || move_count > 1;

    /********************************\
    |   Principal Variation Search   |
    \********************************/

    // If reduced search showed that the move could be good, search it at full depth.
    if (full_search) val = nw_search<Us>(se, alpha, new_depth);

    // If its the first move, or the later move is proven to be good, then do a full window search
    if (pv && (move_count == 1 || (val > alpha && val < beta)))
      val = -negamax<~Us, NT>(se + 1, -beta, -alpha, new_depth);

    undo_move<Us>(se);

    // If we are stopping, return a placeholder score.
    if (stop_.load(std::memory_order::relaxed)) return EvalStop;

    /********************************\
    |       Alpha Beta Pruning       |
    \********************************/

    // If val > alpha, update pv and alpha.
    // If val >= beta (Fail High), this is too good to be played, prune this branch.

    if (val > best) {
      best = val;
      best_move = move;

      if (val > alpha) {
        se->pv.update((se + 1)->pv, best_move);

        if (val >= beta) {
          if (move_count == 1) fail_high_first++;
          fail_high++;
          break;
        }

        alpha = val;
      }
    }

    /********************************\
    |         Move collection        |
    \********************************/

    // Collect all the moves before fail high, and apply maluses to all of them in history

    if (move != best_move && move_count < 32) (is_cap ? captures : quiets).push_back(move);
  }

  if (best >= beta) stats_.update(board_, se, captures, quiets, best_move, depth);

  /********************************\
  |         Mate Detection         |
  \********************************/

  if (move_count == 0) best = se->in_check ? EvalUtils::mated_in(se->ply) : EvalDraw;

  /********************************\
  |            TT write            |
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

template <Colour Us, Worker::NodeType NT> Eval Worker::qsearch(StackEntry *se, Eval alpha, Eval beta) {
  constexpr bool Pv = NT == PV;

  se->pv.clear();
  seldepth_ = std::max(seldepth_, Depth(se->ply + 1));

  if (clock_.stop(nodes_)) return EvalStop;

  // ** TT lookup ** //
  auto [tt_hit, tt_entry] = tt_.probe(board_.key());
  if (tt_entry.key != 0) tt_reads++;

  Move tt_move = NoMove;

  if (tt_hit) {
    tt_hits++;
    TTEntry e = tt_entry.read(se->ply);
    tt_move = e.move;

    if (!Pv && can_tt_cutoff(e, alpha, beta)) return e.value;
  }

  // ** Stand Pat ** //
  // The current eval is the lower bound because we can just not capture
  // anything (assume its not a zugzwang) If lower bound >= beta, then we fail
  // high (opponent has better options) If lower bound > alpha, then we update
  // alpha (the best we can do)
  Eval best = board_.eval();
  if (best >= beta) return best;
  alpha = std::max(alpha, best);

  // ** Main QSearch Loop ** //
  int move_count = 0;
  Move move = NoMove;
  Move best_move = NoMove;

  // Clear killer moves
  (se + 1)->killer.clear();
  MovePicker<Us> mp{board_, se->killer, stats_, tt_move};

  while ((move = mp.next())) {
    move_count++;

    do_move<Us>(se, move);
    Eval val = -qsearch<~Us, NT>(se + 1, -beta, -alpha);
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

        se->pv.update((se + 1)->pv, move);
        if (val >= beta) {
          if (move_count == 1) fail_high_first++;
          fail_high++;
          break;
        }

        alpha = val;
      }
    }
  }

  /********************************\
  |         Mate Detection         |
  \********************************/

  if (move_count == 0 && board_.in_check()) best = EvalUtils::mated_in(se->ply);

  /********************************\
  |            TT write            |
  \********************************/

  // If we fail high, we have a lower bound for how good this pos is.

  // clang-format off
  tt_entry.write(board_.key(), tt_.age(), DepthQS, se->ply,
                 best >= beta ? TTBound::Lower :
                                TTBound::Upper,
                 best_move, 0, best);
  // clang-format on

  return best;
}

} // namespace Lyra
