#include "search_utils.hpp"

#include "move.hpp"
#include "search.hpp"
#include "utils.hpp"

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
|            Pruning conditions            |
|==========================================|
\******************************************/

bool Worker::can_lmr(Depth depth, Move move, bool pv, int move_count) {
  return depth > 2 && move_count > 2 + pv && !is_promo(move) && !is_capture(move);
}

bool Worker::can_nmp(StackEntry *se, Depth depth, Eval eval, Eval beta) {
  return depth >= 2 && (se - 1)->move != NullMove && eval >= beta && !is_win(eval) && !is_loss(beta)
         && board_.has_non_pawn_material(board_.stm());
}

bool Worker::can_see_prune(Depth depth, Eval best, Move move) {
  return !is_terminal(best) && depth <= 10
         && !board_.see(move, is_capture(move) ? -70 * depth : -20 * depth * depth);
}

/******************************************\
|==========================================|
|              Search helpers              |
|==========================================|
\******************************************/

void Worker::reset(const Board &board) {
  board_.copy(board);
  best_move_ = NoMove;
  nodes_     = 0;
  depth_     = 0;
  seldepth_  = 0;

  history_.fill({});
  cap_history_.fill({});
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

MOStats Worker::mostats(StackEntry *se) { return {&se->killer, &history_, &cap_history_}; }

} // namespace Lyra
