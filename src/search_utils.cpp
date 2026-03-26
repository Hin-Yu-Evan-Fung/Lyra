#include "search_utils.hpp"

#include "history.hpp"
#include "move.hpp"
#include "params.hpp"
#include "search.hpp"

#include <print>

namespace Lyra {

using namespace MoveUtils;

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
  best_move_            = NoMove;
  nodes_                = 0;
  depth_                = 0;
  seldepth_             = 0;
  last_best_move_depth_ = 0;

  eval_     = 0;
  avg_eval_ = 0;

  history_     = {};
  cap_history_ = {};
  cont_table_  = {};
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

MOStats Worker::mostats(StackEntry *se) {
  return {&se->killer, &history_, &cap_history_, {(se - 1)->cont, (se - 2)->cont}};
}

void Worker::update_cont_hist(StackEntry *se, Move move, Eval bonus) {
  for (unsigned i = 1; i <= ContSize; ++i) {
    if ((se - i)->move == NoMove) continue;
    PieceTo   p    = piece_to(board_, move);
    ContHist &cont = *(se - i)->cont;
    update_hist(cont[p.pc][p.to], bonus);
  }
}

void Worker::update_all_stats(StackEntry *se, Depth depth, Move best,
                              const std::vector<Move> &captures, const std::vector<Move> &quiets) {
  if (!is_capture(best)) {
    const Eval    bonus = std::min(300 * depth - 250, 1500);
    const PieceTo p     = piece_to(board_, best);
    update_killer(se->killer, best);
    update_hist(history_[p.pc][p.to], bonus);
    update_cont_hist(se, best, bonus);

    for (Move m : quiets) {
      if (m == best) continue;
      const PieceTo p = piece_to(board_, m);
      update_hist(history_[p.pc][p.to], -bonus);
      update_cont_hist(se, m, -bonus);
    }
  }
}

} // namespace Lyra
