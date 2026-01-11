#include "search.hpp"

#include <atomic>
#include <cstdio>
#include <print>

#include "defs.hpp"
#include "movepick.hpp"

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

std::string PVLine::format(bool chess960) {
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
  board_.copy(board);
  pv_    = {};
  nodes_ = 0;
  depth_ = 1;
  ply_   = 0;
}

void Worker::uci_report() {
  std::println(
    "info depth {} seldepth 0 score {} time {} nodes {} nps {} pv {}",
    depth_,
    EvalUtils::format(eval_),
    clock_.elapsed(),
    nodes_,
    nodes_ * 1000 / std::max(clock_.elapsed(), 1UL),
    pv_.format(board_.chess960)
  );
  std::fflush(stdout);
}

void Worker::start(const TimeControl& tc) {
  Colour stm     = board_.stm();
  Move   best_mv = NoMove;

  if (is_main()) clock_.set(stm, tc);

  board_.print();

  while (depth_ < MaxDepth && !clock_.stop_iter(depth_)) {
    Eval alpha = -EvalInf;
    Eval beta  = EvalInf;

    eval_      = board_.stm() == White ? search<White, Root>(board_, pv_, alpha, beta, depth_)
                                       : search<Black, Root>(board_, pv_, alpha, beta, depth_);

    if (stop_.load(std::memory_order::relaxed)) break;

    uci_report();

    best_mv  = pv_.moves[0];
    depth_  += 1;
  }

  std::println("bestmove {}", MoveUtils::format(best_mv, board_.chess960));
  std::fflush(stdout);
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
Eval Worker::search(Board& board, PVLine& pv, Eval alpha, Eval beta, Depth depth) {
  nodes_ += 1;
  pv.clear();

  if (depth == 0) return qsearch<Us, PV>(board, pv, alpha, beta);

  if (NT != Root) {
    if (clock_.stop(nodes_)) return EvalStop;
    if (board.is_draw()) return EvalDraw;
    if (ply_ >= MaxDepth) return board.in_check() ? EvalDraw : board.eval();

    // Our guaranteed score will not be worse than mated in ply.
    alpha = std::max(alpha, EvalUtils::mated_in(ply_));
    // Opponent's worst case scenario will not be worse than mate in ply_ + 1.
    beta = std::min(beta, EvalUtils::mate_in(ply_ + 1));
    // if our guaranteed score is better than the opponent's guaranteed score, no need to continue to search this.
    if (alpha >= beta) return alpha;
  }

  // ** Main Search Loop ** //

  PVLine         child_pv{};
  MovePickState  mps{board, NoMove, depth};
  MovePicker<Us> mp{false, mps};

  Eval best       = -EvalInf;
  int  move_count = 0;
  Move move       = NoMove;

  while ((move = mp.next())) {
    move_count++;

    // ** Recursive Search **
    board.do_move<Us>(move);
    ply_++;

    Eval val = -search<~Us, PV>(board, child_pv, -beta, -alpha, depth - 1);

    ply_--;
    board.undo_move<Us>();

    // If we are stopping, return a placeholder score.
    if (stop_.load(std::memory_order::relaxed)) return EvalStop;
    // If val >= beta (fail high), stop searching this branch,
    // as we won't go down this path and we have a lower bound for the eval
    if (val >= beta) return val;

    best = std::max(best, val);
    // If val > alpha (our global best score), update pv and alpha.
    if (val > alpha) {
      pv.update(child_pv, move);
      alpha = val;
    }
  }

  if (move_count == 0) return board_.in_check() ? EvalUtils::mated_in(ply_) : EvalDraw;

  return best;
}

// ** Quiescence Search **
// Alpha is our guaranteed score
// Beta is our opponent's guaranteed score
template <Colour Us, Worker::NodeType NT>
Eval Worker::qsearch(Board& board, PVLine& pv, Eval alpha, Eval beta) {
  // ** Check Draw ** //

  if (clock_.stop(nodes_)) return EvalStop;

  nodes_ += 1;
  pv.clear();

  PVLine         child_pv{};
  MovePickState  mps{board, NoMove, 0};
  MovePicker<Us> mp{true, mps};

  // ** Stand Pat ** //
  // The current eval is the lower bound because we can just not capture anything (assume its not a zugzwang)
  // If lower bound >= beta, then we fail high (opponent has better options)
  // If lower bound > alpha, then we update alpha (the best we can do)
  Eval best = board.eval();
  if (best >= beta) return best;
  alpha = std::max(alpha, best);

  // ** Main QSearch Loop ** //

  Move move = NoMove;

  while ((move = mp.next())) {
    // ** Recursive Search **
    board.do_move<Us>(move);
    ply_++;

    Eval val = -qsearch<~Us, PV>(board, child_pv, -beta, -alpha);

    ply_--;
    board.undo_move<Us>();

    // If we are stopping, return a placeholder score
    if (stop_.load(std::memory_order::relaxed)) return EvalStop;
    // If val >= beta (their guaranteed score), stop searching this branch,
    // as we won't go down this path and we have a lower bound for the eval
    if (val >= beta) return val;

    best = std::max(best, val);
    // If val > alpha (our guaranteed score), update pv and alpha.
    if (val > alpha) {
      pv.update(child_pv, move);
      alpha = val;
    }

    // This move has a better score than our opponent's guaranteed score - beta,
    // so the opponent won't play this.
    if (val >= beta) return best;
  }

  return best;
}

}  // namespace Lyra
