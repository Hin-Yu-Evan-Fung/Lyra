#include "movepick.hpp"

#include <algorithm>
#include <iterator>

#include "defs.hpp"
#include "move.hpp"
#include "movegen.hpp"

namespace Lyra {

constexpr MovePickStage operator++(MovePickStage& stg) noexcept { return stg = static_cast<MovePickStage>(stg + 1); }

template <Colour Us>
MovePicker<Us>::MovePicker(bool quiescence, const MovePickState& state) : state_(state) {
  if (quiescence) stage_ = Q_TT;
  if (state_.tt_move == NoMove) ++stage_;
}

template <Colour Us>
Eval MovePicker<Us>::score_cap(Move move) {
  static constexpr Eval PIECE_VALS[NPieceType] = {100, 200, 300, 400, 500, 0};
  // MVV LVA
  PieceType attacker = pt_of(state_.board.on(MoveUtils::src(move)));
  PieceType victim   = pt_of(state_.board.on(MoveUtils::dst(move)));

  Eval mvv_lva       = PIECE_VALS[victim] + 6 - PIECE_VALS[attacker] / 100;
  return mvv_lva;
}

template <Colour Us>
Eval MovePicker<Us>::score_quiet(Move move) {
  static Eval score = 0;
  return score++;
}

template <Colour Us>
void MovePicker<Us>::gen_score_cap() {
  start_ptr = 0;
  end_ptr   = MAX_MOVES - 1;
  enum_moves<Us, GenCap>(state_.board, [&](Move move) {
    if (move == state_.tt_move) return;

    if (true) {
      moves_[start_ptr]    = move;
      scores_[start_ptr++] = score_cap(move);
    } else {
      moves_[end_ptr]    = move;
      scores_[end_ptr--] = score_cap(move);
    }
  });
}

template <Colour Us>
void MovePicker<Us>::gen_score_quiet() {
  start_ptr = 0;
  enum_moves<Us, GenQuiet>(state_.board, [&](Move move) {
    if (move == state_.tt_move) return;

    moves_[start_ptr]    = move;
    scores_[start_ptr++] = score_quiet(move);
  });
}

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
  start_ptr--;
  swap(best_idx(0, start_ptr + 1), start_ptr);
  return moves_[start_ptr];
}

template <Colour Us>
Move MovePicker<Us>::pop_back() {
  end_ptr++;
  swap(best_idx(end_ptr, MAX_MOVES), end_ptr);
  return moves_[end_ptr];
}

template <Colour Us>
Move MovePicker<Us>::next() {
  switch (stage_) {
  case TT:
  case Q_TT: ++stage_; return state_.tt_move;
  case INIT_CAP:
  case Q_INIT_CAP:
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
  case Q_CAP:
    if (peek_front()) return pop_front();
    return NoMove;
  };

  return NoMove;
}

template class MovePicker<White>;
template class MovePicker<Black>;

}  // namespace Lyra
