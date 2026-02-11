#include "search/utils.hpp"

#include "core/defs.hpp"
#include "core/move.hpp"
#include "search/movepick.hpp"
#include "search/search.hpp"
#include "utils/tt.hpp"

#include <print>

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
  for (size_t i = 0; i < length; i++) os << MoveUtils::format(moves[i], chess960) << " ";
  return os.str();
}

/******************************************\
|==========================================|
|           Move Ordering Stats            |
|==========================================|
\******************************************/

MOStats Worker::mo_stats(StackEntry *se) {
  return {se->killer, ht_, cht_, {(se - 1)->cont, (se - 2)->cont, (se - 3)->cont, (se - 4)->cont}};
}

void Worker::update_cont(const Board &board, StackEntry *se, Move move, Eval bonus) {
  for (int i = 0; i < ContSize; i++) {
    if (i >= 2 && se->in_check) break;
    if ((se - i)->move) (se - i)->cont->update(board, move, bonus);
  }
}

void Worker::update_hist(const Board &board, StackEntry *se, MoveBuf captures, MoveBuf quiets, Move best_move,
                         Depth depth) {
  const bool is_capture = MoveUtils::is_capture(best_move);
  const Eval bonus = std::max(300 * depth - 200, 2000);

  if (!is_capture) {
    se->killer.update(best_move);
    ht_.update(board, best_move, bonus);
    update_cont(board, se, best_move, bonus);

    for (Move m : quiets) {
      if (!m) continue;
      ht_.update(board, m, -bonus);
      update_cont(board, se, m, -bonus);
    }
  } else
    cht_.update(board, best_move, bonus);

  for (Move m : captures)
    if (m) cht_.update(board, m, -bonus);
}

/******************************************\
|==========================================|
|              Search Helpers              |
|==========================================|
\******************************************/

void Worker::reset(const Board &board) {
  board_.copy(board);
  best_move_ = NoMove;
  nodes_ = 0;
  depth_ = 0;
  seldepth_ = 0;

  ht_.clear();
  cht_.clear();
  cont_tb_.clear();

  tt_reads = 0;
  tt_hits = 0;
  fail_high = 0;
  fail_high_first = 0;
}

void Worker::uci_report(const PVLine &pv) {
  std::println("info depth {} seldepth {} score {} time {} nodes {} nps {} "
               "hashfull {} pv {}",
               depth_ + 1, seldepth_ + 1, EvalUtils::format(eval_), clock_.elapsed(), nodes_,
               nodes_ * 1000 / std::max(clock_.elapsed(), 1UL), tt_.hashfull(), pv.format(board_.chess960));
  std::fflush(stdout);
}

} // namespace Lyra
