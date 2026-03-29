#pragma once

#include "board.hpp"
#include "defs.hpp"
#include "history.hpp"

namespace Lyra {

enum class MPType { Main, QSearch };

enum MPStage {
  MAIN_TT,
  INIT_CAP,
  GOOD_CAP,
  KILLER_1,
  KILLER_2,
  INIT_QUIET,
  QUIET,
  BAD_CAP,

  QSEARCH_TT,
  QSEARCH_INIT,
  QSEARCH,
};

using ContHistBuf = std::array<ContHist *, 2>;

struct MOStats {
  const Killer   *killer;
  const MainHist *hist;
  const CapHist  *cap_hist;
  ContHistBuf     cont_hist_buf;
};

template <Colour Us>
class MovePicker {
public:
  MovePicker(MPType type, const Board &board, MOStats &&stats, Move tt_move, Depth depth);

  Move next();
  int  stage() { return stage_; }
  void skip_quiet() { skip_quiet_ = true; }

  const Board    &board_;
  Move            tt_move_;
  Killer          killer_;
  const MainHist &history_;
  const CapHist  &cap_history_;
  ContHistBuf     cont_hist_buf;

  Depth depth_;
  bool  skip_quiet_;

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

  size_t start_ptr_ = 0;
  size_t end_ptr_   = MaxMoves - 1;
  int    stage_     = MAIN_TT;
};

} // namespace Lyra
