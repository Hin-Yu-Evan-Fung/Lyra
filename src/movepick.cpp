#include "movepick.hpp"

#include <algorithm>
#include <iterator>

#include "defs.hpp"
#include "move.hpp"
#include "movegen.hpp"
#include "search.hpp"

namespace Lyra {

static constexpr Eval PIECE_VALS[NPieceType] = {100, 200, 300, 400, 500, 0};

template <Colour Us>
MovePicker<Us>::MovePicker(const Board& board, Killer* killer, MainHistory* history, Move tt_move, Depth depth)
  : board_(board), killer_(killer), history_(history), tt_move_(tt_move), depth_(depth) {
  stage_ = TT;
}

template <Colour Us>
MovePicker<Us>::MovePicker(const Board& board, Killer* killer, MainHistory* history, Move tt_move)
  : board_(board), killer_(killer), history_(history), tt_move_(tt_move), depth_(0) {
  if (board_.in_check())
    stage_ = EVASION_TT;
  else
    stage_ = QSEARCH_TT;
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
  PieceType attacker = pt_of(board_.on(MoveUtils::src(move)));
  PieceType victim   = pt_of(board_.on(MoveUtils::dst(move)));

  Eval mvv_lva       = PIECE_VALS[victim] + 6 - PIECE_VALS[attacker] / 100;
  return mvv_lva;
}

template <Colour Us>
Eval MovePicker<Us>::score_quiet(Move move) {
  return history_->get(board_, move).eval;
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
  enum_moves<Us, GenCap>(board_, [&](Move move) {
    if (move == tt_move_) return;

    moves_[start_ptr_]    = move;
    scores_[start_ptr_++] = score_cap(move);
  });
}

template <Colour Us>
void MovePicker<Us>::gen_score_quiet() {
  start_ptr_ = 0;

  enum_moves<Us, GenQuiet>(board_, [&](Move move) {
    if (move == tt_move_ || move == killer_->moves[0] || move == killer_->moves[1]) return;

    moves_[start_ptr_]    = move;
    scores_[start_ptr_++] = score_quiet(move);
  });
}

template <Colour Us>
void MovePicker<Us>::gen_score_evasion() {
  start_ptr_ = 0;
  enum_moves<Us, GenAll>(board_, [&](Move move) {
    if (move == tt_move_) return;

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
  Move killer;

  switch (stage_) {
  case TT:
  case QSEARCH_TT:
  case EVASION_TT:
    ++stage_;
    if (board_.is_legal<Us>(tt_move_)) return tt_move_;
    [[fallthrough]];
  case INIT_CAP:
  case QSEARCH_INIT:
    gen_score_cap();
    ++stage_;
    [[fallthrough]];
  case GOOD_CAP:
    if (peek_front()) return pop_front();
    ++stage_;
    [[fallthrough]];
  case KILLER_1:
    ++stage_;
    killer = killer_->moves[0];
    if (killer != tt_move_ && board_.is_legal<Us>(killer)) return killer;
    [[fallthrough]];
  case KILLER_2:
    ++stage_;
    killer = killer_->moves[1];
    if (killer != tt_move_ && board_.is_legal<Us>(killer)) return killer;
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
