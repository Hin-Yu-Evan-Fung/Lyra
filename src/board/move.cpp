#include "core/move.hpp"

#include <cstring>

#include "board/board.hpp"
#include "board/mask.hpp"
#include "core/defs.hpp"
#include "core/zobrist.hpp"

namespace Lyra {

/******************************************\
|==========================================|
|              Do / Undo Move              |
|==========================================|
\******************************************/

void Board::do_move(Move move) {
  return stm_ == White ? do_move<White>(move) : do_move<Black>(move);
}

template <Colour Us> void Board::do_move(Move move) {
  constexpr Colour Them = ~Us;
  constexpr Piece King = make_piece(Us, K);
  constexpr Piece Pawn = make_piece(Us, P);

  const Square src = MoveUtils::src(move);
  const Square dst = MoveUtils::dst(move);
  const Piece pc = on(src);
  const MoveFlag flag = MoveUtils::flag(move);
  Piece cap = on(dst);

  Piece promo;
  Square rook_dst, king_dst, ep;
  bool queen_side;

  // Initialise new state
  Undo *prev = undo_++;
  std::memcpy(undo_, prev, offsetof(Undo, ep));

  // Update new state
  undo_->key ^= (prev->ep != NoSquare) * Zobrist::EP_KEYS[file_of(prev->ep)];
  undo_->ep = NoSquare;
  undo_->cap = cap;
  undo_->move = move;

  // Increment move counters
  undo_->rule50++;
  undo_->ply++;
  gameply_++;

  switch (flag) {
  case Quiet:
    move_piece<true, Us>(src, dst);
    if (pc == Pawn)
      undo_->rule50 = 0;
    break;
  case Cap:
    pop_piece<true, Them>(dst);
    move_piece<true, Us>(src, dst);
    undo_->rule50 = 0;
    break;
  case DoublePush:
    move_piece<true, Us>(src, dst);
    ep = forward<Us>(src);
    undo_->rule50 = 0;
    if (!can_ep<Them>(ep))
      break;
    undo_->ep = ep;
    undo_->key ^= Zobrist::EP_KEYS[file_of(ep)];
    break;
  case KingCastle:
  case QueenCastle:
    queen_side = flag == QueenCastle;
    rook_dst = castling_mask_.rook_dst<Us>(queen_side);
    king_dst = castling_mask_.king_dst<Us>(queen_side);
    pop_piece<true, Us>(src);
    move_piece<true, Us>(dst, rook_dst);
    set_piece<true, Us>(King, king_dst);
    break;
  case PromoCap_Q:
  case PromoCap_R:
  case PromoCap_B:
  case PromoCap_N: //
    pop_piece<true, Them>(dst);
    [[fallthrough]];
  case Promo_Q:
  case Promo_R:
  case Promo_B:
  case Promo_N:
    promo = make_piece(Us, MoveUtils::promoted_pt(move));
    pop_piece<true, Us>(src);
    set_piece<true, Us>(promo, dst);
    undo_->rule50 = 0;
    break;
  case EP:
    ep = forward<Them>(dst);
    cap = make_piece(Them, P);
    move_piece<true, Us>(src, dst);
    pop_piece<true, Them>(ep);
    undo_->cap = cap;
    undo_->rule50 = 0;
    break;
  }

  // Update castling rights and keys
  undo_->key ^= Zobrist::CASTLE_KEYS[undo_->c_rights];
  undo_->c_rights &= castling_mask_.rights[src] & castling_mask_.rights[dst];
  undo_->key ^= Zobrist::CASTLE_KEYS[undo_->c_rights];

  // Update board state
  undo_->key ^= Zobrist::SIDE_KEY;
  stm_ = ~stm_;

  update_masks<Them>();
  update_reps();
}

template <Colour Us> void Board::undo_move() {
  constexpr Colour Them = ~Us;
  constexpr Piece King = make_piece(Us, K);
  constexpr Piece Pawn = make_piece(Us, P);

  const Move move = undo_->move;
  const Piece cap = undo_->cap;
  const Square src = MoveUtils::src(move);
  const Square dst = MoveUtils::dst(move);
  const MoveFlag flag = MoveUtils::flag(move);

  Square rook_dst, king_dst;
  bool queen_side;

  switch (flag) {
  case Quiet:
  case DoublePush: // Move the piece back
    move_piece<false, Us>(dst, src);
    break;
  case Cap:
    move_piece<false, Us>(dst, src);
    set_piece<false, Them>(cap, dst);
    break;
  case KingCastle:
  case QueenCastle:
    queen_side = flag == QueenCastle;
    rook_dst = castling_mask_.rook_dst<Us>(queen_side);
    king_dst = castling_mask_.king_dst<Us>(queen_side);
    pop_piece<false, Us>(king_dst);
    move_piece<false, Us>(rook_dst, dst);
    set_piece<false, Us>(King, src);
    break;
  case Promo_N:
  case Promo_B:
  case Promo_R:
  case Promo_Q:
    pop_piece<false, Us>(dst);
    set_piece<false, Us>(Pawn, src);
    break;
  case PromoCap_N:
  case PromoCap_B:
  case PromoCap_R:
  case PromoCap_Q:
    pop_piece<false, Us>(dst);
    set_piece<false, Us>(Pawn, src);
    set_piece<false, Them>(cap, dst);
    break;
  case EP:
    move_piece<false, Us>(dst, src);
    set_piece<false, Them>(cap, forward<Them>(dst));
    break;
  }

  undo_--;
  stm_ = ~stm_;
  gameply_--;
}

template <Colour Us> void Board::do_null_move() {
  constexpr Colour Them = ~Us;

  // Initialise new state
  Undo *prev = undo_++;
  std::memcpy(undo_, prev, offsetof(Undo, ep));

  // Update new state
  undo_->key ^= (prev->ep != NoSquare) * Zobrist::EP_KEYS[file_of(prev->ep)];
  undo_->ep = NoSquare;
  undo_->cap = NoPiece;
  undo_->move = NullMove;

  undo_->ply++;
  gameply_++;
  // Update board state
  undo_->key ^= Zobrist::SIDE_KEY;
  stm_ = ~stm_;

  update_masks<Them>();
}

template <Colour Us> void Board::undo_null_move() {
  undo_--;
  stm_ = ~stm_;
  gameply_--;
}

template void Board::do_move<White>(Move move);
template void Board::do_move<Black>(Move move);
template void Board::undo_move<White>();
template void Board::undo_move<Black>();

} // namespace Lyra
