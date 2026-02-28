#pragma once

#include "board/board.hpp"
#include "core/defs.hpp"
#include "search/history.hpp"
#include "utils/utils.hpp"

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

using ContHistBuf = NDArray<ContHist *, 4>;

struct MOStats {
  const Killer     &killer;
  const MainHist   &ht;
  const CapHist    &cap_ht;
  const ContHistBuf cont_hb;
};

template <Colour Us> class MovePicker {
public:
  MovePicker(const Board &board, const MOStats &stats, Move tt_move, Depth depth);
  MovePicker(const Board &board, const MOStats &stats, Move tt_move);

  Move next();
  int  stage() { return stage_; }
  void skip_quiet() { skip_quiet_ = true; }

private:
  void gen_score_cap(bool skip_see);
  void gen_score_quiet();
  void gen_score_evasion();
  Eval score_quiet(Move move);
  Eval score_cap(Move move);

  size_t best_idx(size_t start, size_t end);
  void   swap(size_t idx1, size_t idx2);

  bool peek_front() { return start_ptr_ != 0; }
  bool peek_back() { return end_ptr_ != MaxMoves - 1; }
  Move pop_front();
  Move pop_back();

  Move moves_[MaxMoves];
  Eval scores_[MaxMoves];

  const Board      &board_;
  const Killer     &killer_;
  const MainHist   &ht_;
  const CapHist    &cap_ht_;
  const ContHistBuf cont_hb_;
  Move              tt_move_;
  Depth             depth_;

  size_t start_ptr_ = 0;
  size_t end_ptr_   = MaxMoves - 1;
  int    stage_     = MAIN_TT;
  bool   skip_quiet_;
};

} // namespace Lyra
