#pragma once

#include "board/board.hpp"
#include "core/defs.hpp"
#include "search/history.hpp"
#include "utils/utils.hpp"

namespace Lyra {

enum class MovePickerType {
  Main,
  QSearch,
  ProbCut,
};

enum MovePickStage {
  MAIN_TT,
  INIT_CAP,
  GOOD_CAP,
  KILLER_1,
  KILLER_2,
  INIT_QUIET,
  QUIET,
  BAD_CAP,

  QSEARCH_TT,
  INIT_QCAP,
  QCAP,

  PROBCUB_TT,
  INIT_PROBCUT,
  PROBCUT_CAP,
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
  MovePicker(MovePickerType type, const Board &board, const MOStats &stats, Move tt_move,
             Depth depth = DepthQS, Eval threshold = 0);

  Move next();
  int  stage() { return stage_; }
  void skip_quiet() { skip_quiet_ = true; }

private:
  void gen_score_cap();
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

  MovePickerType    type_;
  const Board      &board_;
  const MainHist   &ht_;
  const CapHist    &cap_ht_;
  const ContHistBuf cont_hb_;
  Move              killer0_;
  Move              killer1_;
  Move              tt_move_;
  Depth             depth_;
  Eval              threshold_;

  size_t start_ptr_ = 0;
  size_t end_ptr_   = MaxMoves - 1;
  int    stage_     = MAIN_TT;
  bool   skip_quiet_;
};

} // namespace Lyra
