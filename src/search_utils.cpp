#include "search_utils.hpp"

#include "history.hpp"
#include "move.hpp"
#include "movepick.hpp"
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

bool Worker::should_search_deeper() {
  return depth_ < MaxDepth
         && !clock_.stop_iter(depth_, last_best_move_depth_, avg_eval_, eval_, nodes_, best_move_);
}

MOStats Worker::mostats(StackEntry *se) {
  return {&se->killer, &hist_quiet_, {(se - 1)->cont, (se - 2)->cont}};
}

void Worker::reset(const Board &board) {
  board_.copy(board);
  best_move_            = NoMove;
  nodes_                = 0;
  depth_                = 0;
  seldepth_             = 0;
  last_best_move_depth_ = 0;
  ply_                  = 0;
  ply_from_null_        = 0;

  eval_     = 0;
  avg_eval_ = 0;

  hist_quiet_ = {};
  hist_cont_  = {};
  hist_corr_  = {};

  cutoffs_            = 0;
  first_move_cutoffs_ = 0;
  lmr_searches_       = 0;
  lmr_researches_     = 0;
}

void Worker::uci_report(const PVLine &pv) {
  std::println("info depth {} seldepth {} score {} time {} nodes {} nps {} hashfull {} pv {}",
               depth_ + 1, seldepth_, format_eval(eval_), clock_.elapsed(), nodes_,
               nodes_ * 1000 / std::max(clock_.elapsed(), 1UL), tt_.hashfull(),
               pv.format(board_.chess960));
  std::fflush(stdout);
}

void Worker::report_best_move() {
  std::println("bestmove {}", MoveUtils::format(best_move_, board_.chess960));
  std::fflush(stdout);
}

void Worker::update_hist_cont(StackEntry *se, Move move, Eval bonus) {
  for (unsigned i = 1; i <= ContSize; ++i) {
    if ((se - i)->move == NoMove) continue;
    HistQuiet &cont = *(se - i)->cont;
    update_hist_quiet(cont, board_, move, bonus);
  }
}

void Worker::update_all_stats(StackEntry *se, Depth depth, Move best, std::vector<Move> &captures,
                              std::vector<Move> &quiets) {
  const Eval bonus = std::min(300 * depth - 250, 1500);

  if (!is_capture(best)) {
    update_killer(se->killer, best);
    update_hist_quiet(hist_quiet_, board_, best, bonus);
    update_hist_cont(se, best, bonus);

    for (Move m : quiets) {
      update_hist_quiet(hist_quiet_, board_, m, -bonus);
      update_hist_cont(se, m, -bonus);
    }
  }
}

Eval Worker::adjust_eval(Eval eval) const {
  Eval pcv = hist_corr_[board_.stm()][board_.pawn_key() % CorrHistSize] / 16;
  eval += pcv;
  return std::clamp(eval, -EvalMateBound, EvalMateBound);
}

} // namespace Lyra
