#include "search_utils.hpp"

#include "search.hpp"

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

void Worker::reset(const Board &board) {
  board_.copy(board);
  best_move_ = NoMove;
  nodes_     = 0;
  depth_     = 0;
  seldepth_  = 0;

  history_.fill({});
}

void Worker::uci_report(const PVLine &pv) const {
  std::println("info depth {} seldepth {} score {} time {} nodes {} nps {} "
               "hashfull {} pv {}",
               depth_ + 1, seldepth_ + 1, format_eval(eval_), clock_.elapsed(), nodes_,
               nodes_ * 1000 / std::max(clock_.elapsed(), 1UL), tt_.hashfull(),
               pv.format(board_.chess960));
  std::fflush(stdout);
}

void Worker::report_best_move() const {
  std::println("bestmove {}", MoveUtils::format(best_move_, board_.chess960));
  std::fflush(stdout);
}

MOStats Worker::mostats(StackEntry *se) { return {&se->killer, &history_}; }

} // namespace Lyra
