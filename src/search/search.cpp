#include "search/search.hpp"

#include "core/defs.hpp"
#include "core/move.hpp"
#include "engine/params.hpp"
#include "search/movepick.hpp"
#include "search/utils.hpp"
#include "utils/tt.hpp"
#include "utils/utils.hpp"

#include <print>

namespace Lyra {

using namespace EvalUtils;

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

  std::fflush(stdout);
}

// TODO: Aspiration windows
template <Colour Us> void Worker::aspwin() {

  StackEntry stack[MaxDepth + StackOffset + 1]{};
  StackEntry *se = stack + StackOffset;

  for (int i = StackOffset; i > 0; i--) (se - i)->cont = &cont_tb_[wP][A1]; // Dummy entry
  for (int i = 0; i < MaxDepth; i++) (se + i)->ply = i;

  Eval alpha = -EvalInf;
  Eval beta = EvalInf;
  Eval delta = 10;
  Depth r = 0;

  // Initial guess of the score, because the score should be stable after depth 3
  if (depth_ > 5) {
    alpha = std::max(Eval(eval_ - delta), -EvalInf);
    beta = std::min(Eval(eval_ + delta), EvalInf);
  }

  while (true) {

    Depth reduced = depth_ + 1 - r;
    Eval val = negamax<Us, PV>(se, alpha, beta, reduced, false);

    if (stop_.load(std::memory_order::relaxed)) return;

    /********************************\
    |       Aspiration window        |
    \********************************/

    // Fail low, shift window down and research
    // Fail high, shift window up and research, reduce depth by 1
    // Value inside window, we can confidently update the score
    if (val <= alpha) {
      beta = (alpha + beta) / 2;
      alpha = std::max(val - delta, -EvalInf);
      r = 0;
    } else if (val >= beta) {
      beta = std::min(val + delta, EvalInf);
      if (reduced > 1 && !EvalUtils::is_terminal(val)) r += 1;
    } else {
      eval_ = val;
      break;
    }

    delta *= 1.5f;
  }

  uci_report(se->pv);
  best_move_ = se->pv.moves[0];
}

/******************************************\
|==========================================|
|               Main Search                |
|==========================================|
\******************************************/

// A null window search is used to prove that a move is not better than the upper score.
template <Colour Us> Eval Worker::nw_search(StackEntry *se, Eval upper, Depth depth, bool cutnode) {
  return -negamax<~Us, NonPV>(se + 1, -(upper + 1), -upper, depth, cutnode);
}

// Alpha = our guaranteed score from previous parts of the search
// Beta = opp's guaranteed score from previos parts of the search
template <Colour Us, Worker::NodeType NT>
Eval Worker::negamax(StackEntry *se, Eval alpha, Eval beta, Depth depth, bool cutnode) {
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

    alpha = std::max(alpha, mated_in(se->ply));
    beta = std::min(beta, mate_in(se->ply + 1));
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

    if (!pv && e.depth >= depth && can_tt_cutoff(e, alpha, beta)) return e.value;

    tt_move = e.move;
  }

  Eval eval = board_.eval();

  /********************************\
  |             Pruning            |
  \********************************/

  if (!pv && !se->in_check && is_valid(eval)) {

    /********************************\
    |        Futility Pruning        |
    \********************************/

    // Near a leaf node, prune all moves that are too good to be true.
    if (depth <= 8 && eval - 100 * depth >= beta) return eval;

    /********************************\
    |        Null Move Pruning       |
    \********************************/

    // Prune this node if the following applies:
    // 1. It is safe to do null move pruning(not zugzwang, etc...)
    // 2. Static eval indicates the move is going to fail high.
    // 3. We prove that it will fail high even if we do nothing(null move) using a reduced search.

    if (can_nmp<Us>(se, depth, eval, beta)) {
      Depth r = nmp_reduction(depth);

      do_null_move<Us>(se);
      Eval val = nw_search<Us>(se, beta - 1, depth - r, !cutnode);
      undo_null_move<Us>(se);

      if (val >= beta) return val;
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

  MovePicker<Us> mp{board_, mo_stats(se), tt_move, depth};

  while ((move = mp.next())) {
    move_count++;

    const bool is_cap = MoveUtils::is_capture(move);
    const Depth new_depth = depth - 1;

    if (!pv && !se->in_check) {

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

    do_move<Us>(se, move);

    /********************************\
    |       Late Move Reduction      |
    \********************************/

    Eval val;
    // Assume the first move is the best move.
    // Use a null window with reduced search to prove that later moves are worse.
    if (can_lmr(se, depth, move_count, pv, move)) {
      Depth r = lmr_reduction(depth, move_count);

      r = std::clamp((int)r, 1, new_depth - 1);
      val = nw_search<Us>(se, alpha, new_depth - r, true);

      // If the later moves could be better, research it with full depth.
      full_search = val > alpha && r > 1;
    } else
      full_search = !pv || move_count > 1;

    /********************************\
    |   Principal Variation Search   |
    \********************************/

    // If reduced search showed that the move could be good, search it at full depth.
    if (full_search) val = nw_search<Us>(se, alpha, new_depth, !cutnode);

    // If its the first move, or the later move is proven to be good, then do a full window search
    if (pv && (move_count == 1 || val > alpha))
      val = -negamax<~Us, NT>(se + 1, -beta, -alpha, new_depth, false);

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
      if (val > alpha) {
        best_move = move;
        if (pv) se->pv.update((se + 1)->pv, best_move);
        if (val >= beta) break;
        alpha = val;
      }
    }

    /********************************\
    |         Move collection        |
    \********************************/

    // Collect all the moves before fail high, and apply maluses to all of them in history
    if (move != best_move && move_count < 32) (is_cap ? captures : quiets).push_back(move);
  }

  if (best >= beta) update_hist(board_, se, captures, quiets, best_move, depth);

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

template <Colour Us, Worker::NodeType NT>
Eval Worker::qsearch(StackEntry *se, Eval alpha, Eval beta) {
  constexpr bool pv = NT == PV;

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

    if (!pv && can_tt_cutoff(e, alpha, beta)) return e.value;

    tt_move = e.move;
  }

  Eval eval = board_.eval();
  Eval best = eval;

  // ** Stand Pat ** //
  // The current eval is the lower bound because we can just not capture
  // anything (assume its not a zugzwang) If lower bound >= beta, then we fail
  // high (opponent has better options) If lower bound > alpha, then we update
  // alpha (the best we can do)
  if (best >= beta) return best;
  alpha = std::max(alpha, best);

  // ** Main QSearch Loop ** //
  Move move = NoMove;
  Move best_move = NoMove;
  U16 move_count = 0;

  // Clear killer moves
  (se + 1)->killer.clear();

  MovePicker<Us> mp{board_, mo_stats(se), tt_move};

  while ((move = mp.next())) {
    move_count++;

    if (!pv && !EvalUtils::is_terminal(best)) {

      /********************************\
      |           SEE Pruning          |
      \********************************/

      // Ignore moves with a bad see score,
      // since they are likely to lead to bad positions.
      if (!board_.see(move, -80)) continue;
    }

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
        if (pv) se->pv.update((se + 1)->pv, move);
        if (val >= beta) break;
        alpha = val;
      }
    }
  }

  /********************************\
    |         Mate Detection         |
    \********************************/

  if (move_count == 0 && se->in_check) best = EvalUtils::mated_in(se->ply);

  /********************************\
  |            TT write            |
  \********************************/

  // If we fail high, we have a lower bound for how good this pos is.

  // clang-format off
  tt_entry.write(board_.key(), tt_.age(), DepthQS, se->ply,
                 best >= beta ? TTBound::Lower : TTBound::Upper,
                 best_move, 0, best);
  // clang-format on

  return best;
}

} // namespace Lyra
