#include "movepick.hpp"

#include <algorithm>
#include <iterator>

#include "defs.hpp"
#include "move.hpp"
#include "movegen.hpp"

namespace Lyra {

static constexpr Eval PIECE_VALS[NPieceType] = {100, 200, 300, 400, 500, 0};

template <Colour Us>
MovePicker<Us>::MovePicker(const MovePickState& state) : state_(state) {
  if (state_.board.in_check())
    stage_ = EVASION_TT;
  else if (state_.depth == DepthQS)
    stage_ = QSEARCH_TT;
  else
    stage_ = TT;

  stage_ += !(state_.tt_move && state_.board.is_legal<Us>(state_.tt_move));
}

/******************************************\
|==========================================|
|                  Helpers                 |
|==========================================|
\******************************************/

template <Colour Us>
size_t MovePicker<Us>::best_idx(size_t start, size_t end) {
  return std::distance(scores_, std::max_element(scores_ + start, scores_ + end));
}

template <Colour Us>
void MovePicker<Us>::swap(size_t idx1, size_t idx2) {
  std::swap(moves_[idx1], moves_[idx2]);
  std::swap(scores_[idx1], scores_[idx2]);
}

template <Colour Us>
Move MovePicker<Us>::pop_front() {
  start_ptr_--;
  swap(best_idx(0, start_ptr_ + 1), start_ptr_);
  return moves_[start_ptr_];
}

template <Colour Us>
Move MovePicker<Us>::pop_back() {
  end_ptr_++;
  swap(best_idx(end_ptr_, MaxMoves), end_ptr_);
  return moves_[end_ptr_];
}

/******************************************\
|==========================================|
|                Score Move                |
|==========================================|
\******************************************/

template <Colour Us>
Eval MovePicker<Us>::score_cap(Move move) {
  // MVV LVA
  PieceType attacker = pt_of(state_.board.on(MoveUtils::src(move)));
  PieceType victim   = pt_of(state_.board.on(MoveUtils::dst(move)));

  Eval mvv_lva       = PIECE_VALS[victim] + 6 - PIECE_VALS[attacker] / 100;
  return mvv_lva;
}

template <Colour Us>
Eval MovePicker<Us>::score_quiet(Move move) {
  return 0;
}

/******************************************\
|==========================================|
|             Move Generation              |
|==========================================|
\******************************************/

template <Colour Us>
void MovePicker<Us>::gen_score_cap() {
  start_ptr_ = 0;
  end_ptr_   = MaxMoves - 1;
  enum_moves<Us, GenCap>(state_.board, [&](Move move) {
    if (move == state_.tt_move) return;

    if (true) {
      moves_[start_ptr_]    = move;
      scores_[start_ptr_++] = score_cap(move);
    } else {
      moves_[end_ptr_]    = move;
      scores_[end_ptr_--] = score_cap(move);
    }
  });
}

template <Colour Us>
void MovePicker<Us>::gen_score_quiet() {
  start_ptr_ = 0;
  enum_moves<Us, GenQuiet>(state_.board, [&](Move move) {
    if (move == state_.tt_move) return;

    moves_[start_ptr_]    = move;
    scores_[start_ptr_++] = score_quiet(move);
  });
}

template <Colour Us>
void MovePicker<Us>::gen_score_evasion() {
  start_ptr_ = 0;
  enum_moves<Us, GenAll>(state_.board, [&](Move move) {
    if (move == state_.tt_move) return;

    moves_[start_ptr_] = move;

    if (MoveUtils::is_capture(move))
      scores_[start_ptr_++] = score_cap(move);
    else
      scores_[start_ptr_++] = score_quiet(move);
  });
}

/******************************************\
|==========================================|
|            Next Move Function            |
|==========================================|
\******************************************/

template <Colour Us>
Move MovePicker<Us>::next() {
  switch (stage_) {
  case TT:
  case QSEARCH_TT:
  case EVASION_TT: ++stage_; return state_.tt_move;
  case INIT_CAP:
  case QSEARCH_INIT:
    gen_score_cap();
    ++stage_;
    [[fallthrough]];
  case GOOD_CAP:
    if (peek_front()) return pop_front();
    ++stage_;
    [[fallthrough]];
  case INIT_QUIET:
    gen_score_quiet();
    ++stage_;
    [[fallthrough]];
  case QUIET:
    if (peek_front()) return pop_front();
    ++stage_;
    [[fallthrough]];
  case BAD_CAP:
    if (peek_back()) return pop_back();
    return NoMove;
  case QSEARCH:
    if (peek_front()) return pop_front();
    return NoMove;
  case EVASION_INIT:
    gen_score_evasion();
    ++stage_;
    [[fallthrough]];
  case EVASION:
    if (peek_front()) return pop_front();
    return NoMove;
  };

  return NoMove;
}

template class MovePicker<White>;
template class MovePicker<Black>;

}  // namespace Lyra
