#include "search.hpp"

#include "defs.hpp"
#include "movepick.hpp"

#include <atomic>
#include <cstdio>
#include <print>

namespace Lyra {

/******************************************\
|==========================================|
|                  PVLine                  |
|==========================================|
\******************************************/

void PVLine::update(const PVLine &other, Move best) {
  length   = other.length + 1;
  moves[0] = best;
  std::copy_n(other.moves, other.length, &moves[1]);
}

std::string PVLine::format(bool chess960) const {
  std::ostringstream os;
  for (size_t i = 0; i < length; i++) os << MoveUtils::format(moves[i], chess960) << " ";
  return os.str();
}

/******************************************\
|==========================================|
|              Search Helpers              |
|==========================================|
\******************************************/

bool Worker::should_search_deeper() {
  return depth_ < MaxDepth
         && !clock_.stop_iter(depth_, last_best_move_depth_, avg_eval_, eval_, nodes_, best_move_);
}

void Worker::reset(const Board &board) {
  board_.copy(board);
  best_move_            = NoMove;
  nodes_                = 0;
  depth_                = 0;
  seldepth_             = 0;
  last_best_move_depth_ = 0;
  ply_                  = 0;

  eval_     = 0;
  avg_eval_ = 0;
}

void Worker::uci_report(const PVLine &pv) {
  std::println("info depth {} seldepth 0 score {} time {} nodes {} nps {} pv {}", depth_,
               format_eval(eval_), clock_.elapsed(), nodes_,
               nodes_ * 1000 / std::max(clock_.elapsed(), 1UL), pv.format(board_.chess960));
  std::fflush(stdout);
}

void Worker::report_best_move() {
  std::println("bestmove {}", MoveUtils::format(best_move_, board_.chess960));
  std::fflush(stdout);
}

void Worker::start(const TimeControl &tc) {
  Colour stm = board_.stm();

  if (is_main()) clock_.set(stm, tc);

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
  constexpr bool pv = NT == PV;

  se->pv.clear();

  if (depth <= 0) return qsearch<Us, NT>(se, alpha, beta);

  if (ply_) {
    if (clock_.stop(nodes_)) return EvalStop;
    if (board_.is_draw(ply_)) return EvalDraw;
    if (ply_ >= MaxDepth - 1) return board_.in_check() ? EvalDraw : board_.eval();

    // Our guaranteed score will not be worse than mated in ply.
    alpha = std::max(alpha, mated_in(ply_));
    // Opponent's worst case scenario will not be worse than mate in ply_ + 1.
    beta = std::min(beta, mate_in(ply_ + 1));
    // if our guaranteed score is better than the opponent's guaranteed score, no need to continue
    // to search this.
    if (alpha >= beta) return alpha;
  }

  // ** Main Search Loop ** //

  MovePicker<Us> mp{MPType::Main, board_, {}, NoMove, depth};

  Eval best       = -EvalInf;
  int  move_count = 0;
  Move move       = NoMove;

  while ((move = mp.next())) {
    move_count++;

    // ** Recursive Search **
    board_.do_move<Us>(move);
    ply_++;
    nodes_++;

    Eval val = -search<~Us, PV>(se + 1, -beta, -alpha, depth - 1);

    ply_--;
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
        if (pv) se->pv.update((se + 1)->pv, move);
        if (val >= beta) break;
        alpha = val;
      }
    }
  }

  if (move_count == 0) best = board_.in_check() ? mated_in(ply_) : EvalDraw;

  return best;
}

// ** Quiescence Search **
// Alpha is our guaranteed score
// Beta is our opponent's guaranteed score
template <Colour Us, Worker::NodeType NT>
Eval Worker::qsearch(StackEntry *se, Eval alpha, Eval beta) {
  constexpr bool pv = NT == PV;

  if (clock_.stop(nodes_)) return EvalStop;
  if (board_.is_draw(ply_)) return EvalDraw;
  if (ply_ >= MaxDepth - 1) return board_.in_check() ? EvalDraw : board_.eval();

  se->pv.clear();

  MovePicker<Us> mp{MPType::QSearch, board_, {}, NoMove, DepthQS};

  // ** Stand Pat ** //
  // The current eval is the lower bound because we can just not capture anything (assume its not a
  // zugzwang) If lower bound >= beta, then we fail high (opponent has better options) If lower
  // bound > alpha, then we update alpha (the best we can do)
  Eval best = board_.eval();
  if (best >= beta) return best;
  alpha = std::max(alpha, best);

  // ** Main QSearch Loop ** //

  Move move = NoMove;

  while ((move = mp.next())) {
    // ** Recursive Search **
    board_.do_move<Us>(move);
    ply_++;
    nodes_++;

    Eval val = -qsearch<~Us, PV>(se + 1, -beta, -alpha);

    ply_--;
    board_.undo_move<Us>();

    /********************************\
    |       Alpha Beta Pruning       |
    \********************************/

    // If val > alpha, update pv and alpha.
    // If val >= beta (Fail High), this is too good to be played, prune this branch.

    if (val > best) {
      best = val;
      if (val > alpha) {
        if (pv) se->pv.update((se + 1)->pv, move);
        if (val >= beta) break;
        alpha = val;
      }
    }
  }

  return best;
}

} // namespace Lyra
