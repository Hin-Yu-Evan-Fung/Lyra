#include "search.hpp"

#include <atomic>
#include <cstdio>
#include <print>

#include "defs.hpp"
#include "movepick.hpp"
#include "utils.hpp"

namespace Lyra {

/******************************************\
|==========================================|
|                  PVLine                  |
|==========================================|
\******************************************/

void PVLine::update(const PVLine& other, Move best) {
  length   = other.length + 1;
  moves[0] = best;
  std::copy_n(other.moves, other.length, &moves[1]);
}

std::string PVLine::format(bool chess960) const {
  std::ostringstream os;
  for (size_t i = 0; i < length; i++)
    os << MoveUtils::format(moves[i], chess960) << " ";
  return os.str();
}

/******************************************\
|==========================================|
|              Search Helpers              |
|==========================================|
\******************************************/

void Worker::reset(const Board& board) {
  root_.copy(board);
  best_move_ = NoMove;
  nodes_     = 0;
  depth_     = 0;
  seldepth_  = 0;

  history_.clear();
}

void Worker::uci_report(const PVLine& pv) {
  std::println(
    "info depth {} seldepth {} score {} time {} nodes {} nps {} pv {}",
    depth_ + 1,
    seldepth_ + 1,
    EvalUtils::format(eval_),
    clock_.elapsed(),
    nodes_,
    nodes_ * 1000 / std::max(clock_.elapsed(), 1UL),
    pv.format(root_.chess960)
  );
  std::fflush(stdout);
}

void Worker::start(const TimeControl& tc) {
  Colour stm = root_.stm();

  if (is_main()) clock_.set(stm, tc);

  while (depth_ < MaxDepth && !clock_.stop_iter(depth_)) {
    if (stm == White)
      aspwin<White>();
    else
      aspwin<Black>();

    if (stop_.load(std::memory_order::relaxed)) break;

    depth_ += 1;
  }

  std::println("bestmove {}", MoveUtils::format(best_move_, root_.chess960));
  std::fflush(stdout);
}

// TODO: Aspiration windows
template <Colour Us>
void Worker::aspwin() {
  Eval alpha = -EvalInf;
  Eval beta  = EvalInf;

  StackEntry  stack[MaxDepth + StackOffset]{};
  StackEntry* ss = stack;

  for (int i = 0; i <= MaxDepth + StackOffset; i++)
    (ss + i)->ply = i;

  eval_ = search<Us>(root_, ss, alpha, beta, depth_ + 1);

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
Eval Worker::search(Board& board, StackEntry* ss, Eval alpha, Eval beta, Depth depth) {
  if (depth == 0) return qsearch<Us, PV>(board, ss, alpha, beta);

  ss->pv.clear();
  seldepth_ = std::max(seldepth_, Depth(ss->ply + 1));

  if (NT != Root) {
    if (clock_.stop(nodes_)) return EvalStop;
    if (board.is_draw()) return EvalDraw;
    if (ss->ply >= MaxDepth) return board.in_check() ? EvalDraw : board.eval();

    // Our guaranteed score will not be worse than mated in ply.
    alpha = std::max(alpha, EvalUtils::mated_in(ss->ply));
    // Opponent's worst case scenario will not be worse than mate in ply_ + 1.
    beta = std::min(beta, EvalUtils::mate_in(ss->ply + 1));
    // if our guaranteed score is better than the opponent's guaranteed score, no need to continue to search this.
    if (alpha >= beta) return alpha;
  }

  // ** Main Search Loop ** //

  // Clear killer moves
  (ss + 1)->killer.clear();

  MovePicker<Us> mp{board, &ss->killer, &history_, NoMove, depth};

  Eval best       = -EvalInf;
  int  move_count = 0;
  Move move       = NoMove;
  Move best_move  = NoMove;

  while ((move = mp.next())) {
    move_count++;

    nodes_++;
    // ** Recursive Search ** //
    board.do_move<Us>(move);
    Eval val = -search<~Us, PV>(board, ss + 1, -beta, -alpha, depth - 1);
    board.undo_move<Us>();

    // If we are stopping, return a placeholder score.
    if (stop_.load(std::memory_order::relaxed)) return EvalStop;

    // Update best score
    if (val > best) {
      best      = val;
      best_move = move;
      // If val > alpha (our global best score), update pv and alpha.
      if (val > alpha) {
        // If val >= beta (fail high), stop searching this branch,
        // as we won't go down this path and we have a lower bound for the eval
        if (val >= beta) break;

        ss->pv.update((ss + 1)->pv, move);
        alpha = val;
      }
    }
  }

  if (best_move && !MoveUtils::is_capture(best_move)) {
    ss->killer.update(best_move);
    history_.get(board, best_move).update(300 * depth - 250);
  }

  if (move_count == 0) return board.in_check() ? EvalUtils::mated_in(ss->ply) : EvalDraw;

  return best;
}

// ** Quiescence Search **
// Alpha is our guaranteed score
// Beta is our opponent's guaranteed score
template <Colour Us, Worker::NodeType NT>
Eval Worker::qsearch(Board& board, StackEntry* ss, Eval alpha, Eval beta) {
  if (clock_.stop(nodes_)) return EvalStop;

  ss->pv.clear();
  seldepth_ = std::max(seldepth_, Depth(ss->ply + 1));

  // ** Stand Pat ** //
  // The current eval is the lower bound because we can just not capture anything (assume its not a zugzwang)
  // If lower bound >= beta, then we fail high (opponent has better options)
  // If lower bound > alpha, then we update alpha (the best we can do)
  Eval best = board.eval();
  if (best >= beta) return best;
  alpha = std::max(alpha, best);

  // ** Main QSearch Loop ** //
  Move move       = NoMove;
  int  move_count = 0;

  // Clear killer moves
  (ss + 1)->killer.clear();

  MovePicker<Us> mp{board, &ss->killer, &history_, NoMove};

  while ((move = mp.next())) {
    move_count++;

    nodes_++;
    // ** Recursive Search ** //
    board.do_move<Us>(move);
    Eval val = -qsearch<~Us, PV>(board, ss + 1, -beta, -alpha);
    board.undo_move<Us>();

    // If we are stopping, return a placeholder score
    if (stop_.load(std::memory_order::relaxed)) return EvalStop;

    // Update best score
    if (val > best) {
      best = val;
      // If val > alpha (our global best score), update pv and alpha.
      if (val > alpha) {
        // If val >= beta (fail high), stop searching this branch,
        // as we won't go down this path and we have a lower bound for the eval
        if (val >= beta) break;

        ss->pv.update((ss + 1)->pv, move);
        alpha = val;
      }
    }
  }

  if (move_count == 0 && board.in_check()) return EvalUtils::mated_in(ss->ply);

  return best;
}

}  // namespace Lyra
