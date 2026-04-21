#include "movepick.hpp"

#include "defs.hpp"
#include "move.hpp"
#include "movegen.hpp"

#include <algorithm>
#include <iterator>

namespace Lyra {

template <Colour Us>
MovePicker<Us>::MovePicker(MPType type, const Board &board, MOStats &&mostats, Move tt_move,
                           Depth depth)
    : board_(board)
    , tt_move_(tt_move)
    , depth_(depth)
    , skip_quiet_(false)
    , type_(type) {

  switch (type) {
  case MPType::Main: stage_ = MAIN_TT; break;
  case MPType::QSearch: stage_ = Q_TT; break;
  }

  if (type == MPType::QSearch && !is_capture(tt_move_)) {
    tt_move_ = NoMove;
  }
}

template <Colour Us>
Eval MovePicker<Us>::score_cap(Move move) {
  static constexpr Eval PIECE_VALS[NPieceType] = {100, 200, 300, 400, 500, 0};
  // MVV LVA
  PieceType attacker = board_.moved(move);
  PieceType victim   = board_.captured(move);

  Eval mvv_lva = PIECE_VALS[victim] + 6 - PIECE_VALS[attacker] / 100;
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
  end_ptr   = MaxMoves - 1;
  enum_moves<Us, GenCap>(board_, [&](Move move) {
    if (move == tt_move_) return;

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
  enum_moves<Us, GenQuiet>(board_, [&](Move move) {
    if (move == tt_move_) return;

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
  swap(best_idx(end_ptr, MaxMoves), end_ptr);
  return moves_[end_ptr];
}

template <Colour Us>
Move MovePicker<Us>::next() {
  switch (stage_) {
  case MAIN_TT:
  case Q_TT:
    ++stage_;
    if (tt_move_) return tt_move_;
    return next();
  case INIT_CAP:
  case Q_INIT:
    gen_score_cap();
    ++stage_;
    return next();
  case GOOD_CAP:
    if (peek_front()) return pop_front();
    ++stage_;
    return next();
  case INIT_QUIET:
    if (!skip_quiet_) gen_score_quiet();
    ++stage_;
    return next();
  case QUIET:
    if (!skip_quiet_ && peek_front()) return pop_front();
    ++stage_;
    return next();
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

} // namespace Lyra
