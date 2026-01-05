#include "search.hpp"

#include <atomic>
#include <print>

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

void Worker::reset(std::string fen) {
  board_.set(fen);
  pv_    = {};
  nodes_ = 0;
  depth_ = 1;
  nodes_ = 0;
}

void Worker::uci_report() {
  std::println(
    "info depth {} seldepth 0 score cp {} time {} nodes {} nps {} pv {}\n",
    depth_,
    eval_,
    clock_.elapsed(),
    nodes_,
    nodes_ * 1000 / std::max(clock_.elapsed(), 1UL),
    pv_.format(board_.chess960)
  );
}

void Worker::start(const TimeControl& tc) {
  Colour stm   = board_.stm();

  Move best_mv = NoMove;

  if (is_main()) clock_.set(stm, tc);

  while (depth_ < MAX_DEPTH && !clock_.stop_iter(depth_)) {
    Eval alpha = -EVAL_INF;
    Eval beta  = EVAL_INF;

    eval_      = search(board_, pv_, alpha, beta, depth_);

    if (stop_.load(std::memory_order::relaxed)) break;

    uci_report();

    best_mv  = pv_.moves[0];
    depth_  += 1;
  }

  std::println("bestmove {}", MoveUtils::format(best_mv, board_.chess960));
}

/******************************************\
|==========================================|
|               Main Search                |
|==========================================|
\******************************************/

Eval Worker::search(Board& board, PVLine& pv, Eval alpha, Eval beta, Depth depth) {
  return board.stm() == White ? search<White, Root>(board_, pv_, alpha, beta, depth_)
                              : search<Black, Root>(board_, pv_, alpha, beta, depth_);
}

template <Colour Us, Worker::NodeType NT>
Eval Worker::search(Board& board, PVLine& pv, Eval alpha, Eval beta, Depth depth) {
  if (clock_.stop(nodes_)) return EVAL_DRAW;

  nodes_ += 1;
  pv.clear();

  if (depth == 0) return qsearch<Us, PV>(board, pv, alpha, beta);

  PVLine child_pv{};

  MovePickState  mps{board, NoMove, depth};
  MovePicker<Us> mp{false, mps};

  Eval best = -EVAL_INF;
  Move mv   = NoMove;

  while ((mv = mp.next())) {
    board.do_move<Us>(mv);
    Eval val = -search<~Us, PV>(board, child_pv, -beta, -alpha, depth - 1);
    board.undo_move<Us>();

    if (stop_.load(std::memory_order::relaxed)) return EVAL_DRAW;

    if (val > best) {
      best = val;

      if (val > alpha) {
        pv.update(child_pv, mv);
        alpha = val;
      }
    }

    if (val >= beta) return best;
  }

  return best;
}

template <Colour Us, Worker::NodeType NT>
Eval Worker::qsearch(Board& board, PVLine& pv, Eval alpha, Eval beta) {
  if (clock_.stop(nodes_)) return EVAL_DRAW;

  nodes_ += 1;
  pv.clear();

  PVLine child_pv{};

  MovePickState  mps{board, NoMove, 0};
  MovePicker<Us> mp{true, mps};

  Eval best = board.compute_incr_eval();
  if (best >= beta) return best;
  if (best > alpha) alpha = best;

  Move mv = NoMove;

  while ((mv = mp.next())) {
    board.do_move<Us>(mv);
    Eval val = -qsearch<~Us, PV>(board, child_pv, -beta, -alpha);
    board.undo_move<Us>();

    if (stop_.load(std::memory_order::relaxed)) return EVAL_DRAW;

    if (val > best) {
      best = val;

      if (val > alpha) {
        pv.update(child_pv, mv);
        alpha = val;
      }
    }

    if (val >= beta) return best;
  }

  return best;
}

}  // namespace Lyra
