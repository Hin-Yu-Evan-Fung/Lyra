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

  Q_TT,
  Q_INIT,
  Q_CAP
};

// Contains all info required for MovePicker
struct MOStats {
  const Killer    *killer;
  const HistQuiet *hist_quiet;
};

template <Colour Us>
class MovePicker {
public:
  MovePicker(MPType type, const Board &board, MOStats &&stats, Move tt_move, Depth depth);

  Move next();
  bool skip_quiet_;

private:
  void gen_score_cap();
  void gen_score_quiet();
  Eval score_quiet(Move move);
  Eval score_cap(Move move);

  size_t best_idx(size_t start, size_t end);
  void   swap(size_t idx1, size_t idx2);

  bool peek_front() { return start_ptr != 0; }
  bool peek_back() { return end_ptr != MaxMoves - 1; }
  Move pop_front();
  Move pop_back();

  Move moves_[MaxMoves];
  Eval scores_[MaxMoves];

  size_t start_ptr = 0;
  size_t end_ptr   = MaxMoves - 1;

  const Board &board_;
  Move         tt_move_;
  Depth        depth_;

  Killer           killer_;
  const HistQuiet &hist_quiet_;

  MPType   type_;
  unsigned stage_ = MAIN_TT;
};

} // namespace Lyra
