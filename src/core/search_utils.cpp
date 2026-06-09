#include "search_utils.hpp"

#include "history.hpp"
#include "move.hpp"
#include "movepick.hpp"
#include "params.hpp"
#include "search.hpp"

#include <cstring>
#include <iostream>

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

// Returns a structure containing all the history tables for move ordering
MOStats Worker::mostats(StackEntry *se) {
  return {&se->killer, hist_quiet_.get(), {(se - 1)->cont, (se - 2)->cont}};
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

  std::memset(hist_quiet_.get(), 0, sizeof(HistQuiet));
  std::memset(hist_cont_.get(), 0, sizeof(HistCont));
  std::memset(hist_corr_.get(), 0, sizeof(HistCorr));

  cutoffs_            = 0;
  first_move_cutoffs_ = 0;
  lmr_searches_       = 0;
  lmr_researches_     = 0;
}

void Worker::uci_report(const PVLine &pv) {

  PrintInfo info;

  info.depth    = depth_ + 1;
  info.seldepth = seldepth_;
  info.eval     = eval_;
  info.time     = clock_.elapsed();
  info.nodes    = nodes_;
  info.nps      = info.nodes * 1000 / (info.time == 0 ? 1 : info.time);
  info.hashfull = tt_.hashfull();
  info.pv       = pv.format(board_.chess960);

  if (callbacks_.on_depth_finished) callbacks_.on_depth_finished(info);
}

void Worker::report_best_move() {
  if (callbacks_.on_best_move) callbacks_.on_best_move(best_move_);
}

void Worker::update_hist_cont(StackEntry *se, Move move, Eval bonus) {
  for (unsigned i = 1; i <= ContSize; ++i) {
    if ((se - i)->move == NoMove) continue;
    update_hist_quiet((se - i)->cont, board_, move, bonus);
  }
}

// Apply bonus to the best move and maluses to worse moves that came before it.
void Worker::update_all_stats(StackEntry *se, Depth depth, Move best, std::vector<Move> &captures,
                              std::vector<Move> &quiets) {
  const Eval bonus = std::min(300 * depth - 250, 1500);

  if (!is_capture(best)) {
    update_killer(se->killer, best);
    update_hist_quiet(hist_quiet_.get(), board_, best, bonus);
    update_hist_cont(se, best, bonus);

    for (Move m : quiets) {
      update_hist_quiet(hist_quiet_.get(), board_, m, -bonus);
      update_hist_cont(se, m, -bonus);
    }
  }
}

// Adjust eval using correction history
Eval Worker::adjust_eval(Eval eval) const {
  Eval pcv = (*hist_corr_)[board_.stm()][board_.pawn_key() % CorrHistSize] / 16;
  eval += pcv;
  return std::clamp(eval, -EvalMateBound, EvalMateBound);
}

// Check if the position is an improvement compared to a move or two ago
bool Worker::is_improving(StackEntry *se) const {
  if (ply_ >= 2 && (se - 2)->eval != -EvalInf) {
    return se->eval > (se - 2)->eval;
  } else if (ply_ >= 4 && (se - 4)->eval != -EvalInf) {
    return se->eval > (se - 4)->eval;
  } else {
    return true;
  }
}

} // namespace Lyra
