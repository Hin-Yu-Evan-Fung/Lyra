#include "search/search.hpp"

#include <print>

#include "core/defs.hpp"
#include "core/move.hpp"
#include "search/movepick.hpp"
#include "search/utils.hpp"
#include "utils/tt.hpp"
#include "utils/utils.hpp"

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

    if (stop_.load(std::memory_order::relaxed))
      break;

    depth_ += 1;
  }

  std::println("bestmove {}", MoveUtils::format(best_move_, board_.chess960));
  std::println("TT read/hit %: {}%", (float)tt_hits * 100 / (float)tt_reads);
  std::println("First move fail high %: {}%",
               (float)fail_high_first * 100 / (float)fail_high);
  std::println("LMR research %: {}%",
               (float)lmr_researches * 100 / (float)lmr_searches);
  std::fflush(stdout);
}

// TODO: Aspiration windows
template <Colour Us> void Worker::aspwin() {
  Eval alpha = -EvalInf;
  Eval beta = EvalInf;

  StackEntry stack[MaxDepth + StackOffset]{};
  StackEntry *ss = stack;

  for (int i = 0; i < MaxDepth + StackOffset; i++)
    (ss + i)->ply = i;

  eval_ = search<Us, PV>(ss, alpha, beta, depth_ + 1);

  if (stop_.load(std::memory_order::relaxed))
    return;

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
Eval Worker::search(StackEntry *ss, Eval alpha, Eval beta, Depth depth) {
  constexpr bool pv = NT == PV;

  if (depth == 0)
    return qsearch<Us, NT>(ss, alpha, beta);

  ss->pv.clear();
  seldepth_ = std::max(seldepth_, Depth(ss->ply + 1));

  if (ss->ply) {
    if (clock_.stop(nodes_))
      return EvalStop;
    if (board_.is_draw(ss->ply))
      return EvalDraw;
    if (ss->ply >= MaxDepth)
      return board_.in_check() ? EvalDraw : board_.eval();

    // Our guaranteed score will not be worse than mated in ply.
    alpha = std::max(alpha, EvalUtils::mated_in(ss->ply));
    // Opponent's worst case scenario will not be worse than mate in ply_ + 1.
    beta = std::min(beta, EvalUtils::mate_in(ss->ply + 1));
    // if our guaranteed score is better than the opponent's guaranteed score,
    // no need to continue to search this.
    if (alpha >= beta)
      return alpha;
  }

  // ** TT lookup ** //
  auto [tt_hit, tt_entry] = tt_.probe(board_.key());
  if (tt_entry.key != 0)
    tt_reads++;

  Move tt_move = NoMove;
  TTBound tt_bound = TTBound::Upper;

  if (tt_hit) {
    tt_hits++;
    TTEntry e = tt_entry.read(ss->ply);
    tt_move = e.move;

    if (!pv && e.depth >= depth && can_tt_cutoff(e, alpha, beta))
      return e.value;
  }

  // ** Null Move Pruning ** //

  // ** Main Search Loop ** //

  Eval val, best = -EvalInf;
  bool full_search;
  int move_count = 0;
  Move move = NoMove;
  Move best_move = NoMove;

  MoveBuf captures(32), quiets(32);

  // Clear killer moves
  (ss + 1)->killer.clear();
  MovePicker<Us> mp{board_, ss->killer, stats_, tt_move, depth};

  while ((move = mp.next())) {
    move_count++;

    nodes_++;
    // ** Recursive Search ** //
    board_.do_move<Us>(move);

    const bool is_cap = MoveUtils::is_capture(move);

    // ** Late Move Reduction ** //
    // Assume the first move is the best move.
    // Use a null window with reduced search to prove that later moves are
    // worse.
    if (depth >= 2 && move_count > 1 + pv) {
      Depth r = lmr_reduction(depth, move_count);
      // Reduce less for killers and good captures
      r -= mp.stage() < INIT_QUIET;
      // Reduce less for checks
      r -= board_.in_check();

      r = std::max(r, Depth(1));

      val = -search<~Us, NonPV>(ss + 1, -alpha - 1, -alpha, depth - 1 - r);

      lmr_searches++;
      // If the later moves could be better, research it with full depth.
      full_search = val > alpha && r > 1;
      lmr_researches += full_search;
    } else
      full_search = !pv || move_count > 1;

    // ** Principal Variation Search ** //
    // If reduced search showed that the move could be good, search it at full
    // depth.
    if (full_search)
      val = -search<~Us, NonPV>(ss + 1, -alpha - 1, -alpha, depth - 1);

    // If its the first move, or the later move is proven to be good, then do a
    // full search
    if (pv && (move_count == 1 || (val > alpha && val < beta))) {
      lmr_researches += move_count != 1;
      val = -search<~Us, NT>(ss + 1, -beta, -alpha, depth - 1);
    }

    board_.undo_move<Us>();

    // If we are stopping, return a placeholder score.
    if (stop_.load(std::memory_order::relaxed))
      return EvalStop;

    // Update best score
    if (val > best) {
      best = val;

      // If val > alpha (our global best score), update pv and alpha.
      if (val > alpha) {
        best_move = move;

        if constexpr (pv)
          tt_bound = TTBound::Exact;

        ss->pv.update((ss + 1)->pv, best_move);
        // If val >= beta (fail high), stop searching this branch,
        // as we won't go down this path and we have a lower bound for the eval
        if (val >= beta) {
          // If we fail high, then we have a lower bound for how good this
          // position is.
          tt_bound = TTBound::Lower;
          if (move_count == 1)
            fail_high_first++;
          fail_high++;
          break;
        }

        alpha = val;
      }
    }

    if (move != best_move && move_count < 32)
      (is_cap ? captures : quiets).push_back(move);
  }

  if (best >= beta)
    stats_.update(board_, ss, captures, quiets, best_move, depth);

  if (move_count == 0)
    best = board_.in_check() ? EvalUtils::mated_in(ss->ply) : EvalDraw;

  tt_entry.write(board_.key(), tt_.age(), depth, ss->ply, tt_bound, best_move,
                 0, best);

  return best;
}

// ** Quiescence Search **
// Alpha is our guaranteed score
// Beta is our opponent's guaranteed score
template <Colour Us, Worker::NodeType NT>
Eval Worker::qsearch(StackEntry *ss, Eval alpha, Eval beta) {
  constexpr bool Pv = NT == PV;

  ss->pv.clear();
  seldepth_ = std::max(seldepth_, Depth(ss->ply + 1));

  if (clock_.stop(nodes_))
    return EvalStop;

  // ** TT lookup ** //
  auto [tt_hit, tt_entry] = tt_.probe(board_.key());
  if (tt_entry.key != 0)
    tt_reads++;

  Move tt_move = NoMove;
  TTBound tt_bound = TTBound::Upper;

  if (tt_hit) {
    tt_hits++;
    TTEntry e = tt_entry.read(ss->ply);
    tt_move = e.move;

    if (!Pv && can_tt_cutoff(e, alpha, beta))
      return e.value;
  }

  // ** Stand Pat ** //
  // The current eval is the lower bound because we can just not capture
  // anything (assume its not a zugzwang) If lower bound >= beta, then we fail
  // high (opponent has better options) If lower bound > alpha, then we update
  // alpha (the best we can do)
  Eval best = board_.eval();
  if (best >= beta)
    return best;
  alpha = std::max(alpha, best);

  // ** Main QSearch Loop ** //
  int move_count = 0;
  Move move = NoMove;
  Move best_move = NoMove;

  // Clear killer moves
  (ss + 1)->killer.clear();
  MovePicker<Us> mp{board_, ss->killer, stats_, tt_move};

  while ((move = mp.next())) {
    move_count++;

    nodes_++;
    // ** Recursive Search ** //
    board_.do_move<Us>(move);
    Eval val = -qsearch<~Us, PV>(ss + 1, -beta, -alpha);
    board_.undo_move<Us>();

    // If we are stopping, return a placeholder score
    if (stop_.load(std::memory_order::relaxed))
      return EvalStop;

    // Update best score
    if (val > best) {
      best = val;
      // If val > alpha (our global best score), update pv and alpha.
      if (val > alpha) {
        best_move = move;

        ss->pv.update((ss + 1)->pv, move);
        // If val >= beta (fail high), stop searching this branch,
        // as we won't go down this path and we have a lower bound for the eval
        if (val >= beta) {
          // If we fail high, then we have a lower bound for how good this
          // position is.
          tt_bound = TTBound::Lower;
          if (move_count == 1)
            fail_high_first++;
          fail_high++;
          break;
        }

        alpha = val;
      }
    }
  }

  if (move_count == 0 && board_.in_check())
    best = EvalUtils::mated_in(ss->ply);

  tt_entry.write(board_.key(), tt_.age(), DepthQS, ss->ply, tt_bound, best_move,
                 0, best);

  return best;
}

} // namespace Lyra
