#pragma once

#include "board.hpp"
#include "defs.hpp"

namespace Lyra {

enum MovePickStage {
  TT,
  INIT_CAP,
  GOOD_CAP,
  INIT_QUIET,
  QUIET,
  BAD_CAP,

  Q_TT,
  Q_INIT_CAP,
  Q_CAP
};

// Contains all info required for MovePicker
struct MovePickState {
  const Board& board;
  Move         tt_move;
  Depth        depth;
};

template <Colour Us>
class MovePicker {
 public:
  MovePicker(bool quiescence, const MovePickState& state);

  Move next();

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

  const MovePickState& state_;
  MovePickStage        stage_ = TT;
};

}  // namespace Lyra
