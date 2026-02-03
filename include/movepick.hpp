#pragma once

#include "board.hpp"
#include "defs.hpp"
#include "search.hpp"

namespace Lyra {

enum MovePickStage {
  MAIN_TT,
  INIT_CAP,
  GOOD_CAP,
  KILLER_1,
  KILLER_2,
  INIT_QUIET,
  QUIET,
  BAD_CAP,

  EVASION_TT,
  EVASION_INIT,
  EVASION,

  QSEARCH_TT,
  INIT_QCAP,
  QCAP,
};

template <Colour Us> class MovePicker {
public:
  MovePicker(const Board &board, Killer *killer, MainHistory *history,
             Move tt_move, Depth depth);
  MovePicker(const Board &board, Killer *killer, MainHistory *history,
             Move tt_move);

  Move next();
  int stage() { return stage_; }

  const Board &board_;
  Killer *killer_;
  MainHistory *history_;
  Move tt_move_;
  Depth depth_;

private:
  void gen_score_cap(bool skip_see);
  void gen_score_quiet();
  void gen_score_evasion();
  Eval score_quiet(Move move);
  Eval score_cap(Move move);

  size_t best_idx(size_t start, size_t end);
  void swap(size_t idx1, size_t idx2);

  bool peek_front() { return start_ptr_ != 0; }
  bool peek_back() { return end_ptr_ != MaxMoves - 1; }
  Move pop_front();
  Move pop_back();

  Move moves_[MaxMoves];
  Eval scores_[MaxMoves];

  size_t start_ptr_ = 0;
  size_t end_ptr_ = MaxMoves - 1;
  int stage_ = MAIN_TT;
};

} // namespace Lyra
