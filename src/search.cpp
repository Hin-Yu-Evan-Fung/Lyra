#include "search.hpp"

#include <atomic>
#include <cstdio>
#include <print>

#include "defs.hpp"
#include "move.hpp"
#include "movepick.hpp"
#include "tt.hpp"
#include "utils.hpp"

namespace Lyra {

/******************************************\
|==========================================|
|                  PVLine                  |
|==========================================|
\******************************************/

void PVLine::update(const PVLine &other, Move best) {
  length = other.length + 1;
  moves[0] = best;
  std::copy_n(other.moves, other.length, moves + 1);
}

std::string PVLine::format(bool chess960) const {
  std::ostringstream os;
  for (size_t i = 0; i <= length && moves[i]; i++)
    os << MoveUtils::format(moves[i], chess960) << " ";
  return os.str();
}

/******************************************\
|==========================================|
|              Search Helpers              |
|==========================================|
\******************************************/

constexpr bool can_tt_cutoff(TTBound bound, Eval value, Eval alpha, Eval beta) {
  switch (bound) {
    // If current node has been search with a full window to
    // a higher depth, we can use it.
  case TTBound::Exact:
    return true;
    // If current node has a proven upper bound, and the upper bound is worse
    // than alpha, then this move is to good to be true.
  case TTBound::Upper:
    return value <= alpha;
    // If current node has a proven lower bound, and the lower bound is better
    // than beta, then this move is too good to be true.
  case TTBound::Lower:
    return value >= beta;
  case TTBound::None:
    return false;
  }
  return false;
}

void Worker::reset(const Board &board) {
  board_.copy(board);
  best_move_ = NoMove;
  nodes_ = 0;
  depth_ = 0;
  seldepth_ = 0;
  history_.clear();

  tt_reads = 0;
  tt_hits = 0;
}

void Worker::uci_report(const PVLine &pv) {
  std::println("info depth {} seldepth {} score {} time {} nodes {} nps {} "
               "hashfull {} pv {}",
               depth_ + 1, seldepth_ + 1, EvalUtils::format(eval_),
               clock_.elapsed(), nodes_,
               nodes_ * 1000 / std::max(clock_.elapsed(), 1UL), tt_.hashfull(),
               pv.format(board_.chess960));
  std::fflush(stdout);
}

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
  std::println("TT read/hit %: {}\n", (float)tt_hits * 100 / (float)tt_reads);
  std::fflush(stdout);
}

// TODO: Aspiration windows
template <Colour Us> void Worker::aspwin() {
  Eval alpha = -EvalInf;
  Eval beta = EvalInf;

  StackEntry stack[MaxDepth + StackOffset]{};
  StackEntry *ss = stack;

  for (int i = 0; i <= MaxDepth + StackOffset; i++)
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
  constexpr bool Pv = NT == PV;

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

    if (!Pv && e.depth >= depth && can_tt_cutoff(e.bound, e.value, alpha, beta))
      return e.value;
  }

  // ** Main Search Loop ** //

  Eval val, best = -EvalInf;
  bool full_search;
  int move_count = 0;
  Move move = NoMove;
  Move best_move = NoMove;

  // Clear killer moves
  (ss + 1)->killer.clear();
  MovePicker<Us> mp{board_, &ss->killer, &history_, tt_move, depth};

  while ((move = mp.next())) {
    move_count++;

    nodes_++;
    // ** Recursive Search ** //
    board_.do_move<Us>(move);

    // ** Late Move Reduction ** //
    // Assume the first move is the best move.
    // Use a null window with reduced search to prove that later moves are
    // worse.
    if (depth >= 3 && move_count > 4 + Pv && mp.stage() != GOOD_CAP) {
      val = -search<~Us, NonPV>(ss + 1, -alpha - 1, -alpha, depth - 2);
      // If the later moves could be better, research it with full depth.
      full_search = val > alpha;
    } else
      full_search = !Pv || move_count > 1;

    // ** Principal Variation Search ** //
    // If reduced search showed that the move could be good, search it at full
    // depth.
    if (full_search)
      val = -search<~Us, NonPV>(ss + 1, -alpha - 1, -alpha, depth - 1);

    // If its the first move, or the later move is proven to be good, then do a
    // full search
    if (Pv && (move_count == 1 || (val > alpha && val < beta)))
      val = -search<~Us, PV>(ss + 1, -beta, -alpha, depth - 1);

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

        if constexpr (Pv)
          tt_bound = TTBound::Exact;

        ss->pv.update((ss + 1)->pv, best_move);
        // If val >= beta (fail high), stop searching this branch,
        // as we won't go down this path and we have a lower bound for the eval
        if (val >= beta) {
          // If we fail high, then we have a lower bound for how good this
          // position is.
          tt_bound = TTBound::Lower;
          break;
        }

        alpha = val;
      }
    }
  }

  if (best_move && !MoveUtils::is_capture(best_move)) {
    ss->killer.update(best_move);
    history_.get(board_, best_move).update(300 * depth - 250);
  }

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

    if (!Pv && can_tt_cutoff(e.bound, e.value, alpha, beta))
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
  MovePicker<Us> mp{board_, &ss->killer, &history_, tt_move};

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
