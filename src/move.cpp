#include "move.hpp"

#include "board.hpp"
#include "defs.hpp"
#include "mask.hpp"
#include "zobrist.hpp"

namespace Lyra {

/******************************************\
|==========================================|
|              Do / Undo Move              |
|==========================================|
\******************************************/

void Board::do_move(Move move) { return stm_ == White ? do_move<White>(move) : do_move<Black>(move); }

template <Colour Us>
void Board::do_move(Move move) {
  constexpr Colour Them = ~Us;
  constexpr Piece  King = make_piece(Us, K);
  constexpr Piece  Pawn = make_piece(Us, P);
  const Square     src  = MoveUtils::src(move);
  const Square     dst  = MoveUtils::dst(move);
  const Piece      pc   = on(src);
  const MoveFlag   flag = MoveUtils::flag(move);
  Piece            promo;
  Square           rook_dst, king_dst, ep_sq;
  bool             queen_side;

  half_mv_++;

  // Initialise new state
  Undo* prev         = state_++;
  state_->mv         = move;
  state_->castling   = prev->castling;
  state_->fifty_mv   = prev->fifty_mv + 1;
  state_->game_phase = prev->game_phase;
  state_->psq        = prev->psq;

  Piece cap          = on(dst);
  state_->cap        = cap;
  state_->key        = prev->key;
  // Update key if enpassant is reset
  state_->key ^= (prev->ep != NoSquare) * Zobrist::EP_KEYS[file_of(prev->ep)];
  state_->ep   = NoSquare;

  switch (flag) {
  case Quiet:
    move_piece<true, Us>(src, dst);
    if (pc == Pawn) state_->fifty_mv = 0;
    break;
  case Cap:
    pop_piece<true, Them>(dst);
    move_piece<true, Us>(src, dst);
    state_->fifty_mv = 0;
    break;
  case DoublePush:
    move_piece<true, Us>(src, dst);
    state_->ep        = forward<Us>(src);
    state_->key      ^= Zobrist::EP_KEYS[file_of(src)];
    state_->fifty_mv  = 0;
    break;
  case KingCastle:
  case QueenCastle:
    queen_side = flag == QueenCastle;
    rook_dst   = castling_mask_.rook_dst<Us>(queen_side);
    king_dst   = castling_mask_.king_dst<Us>(queen_side);
    pop_piece<true, Us>(src);
    move_piece<true, Us>(dst, rook_dst);
    set_piece<true, Us>(King, king_dst);
    break;
  case PromoCap_Q:
  case PromoCap_R:
  case PromoCap_B:
  case PromoCap_N:  //
    pop_piece<true, Them>(dst);
    [[fallthrough]];
  case Promo_Q:
  case Promo_R:
  case Promo_B:
  case Promo_N:
    promo = make_piece(Us, MoveUtils::promoted_pt(move));
    pop_piece<true, Us>(src);
    set_piece<true, Us>(promo, dst);
    state_->fifty_mv = 0;
    break;
  case EP:
    ep_sq = forward<Them>(dst);
    cap   = make_piece(Them, P);
    move_piece<true, Us>(src, dst);
    pop_piece<true, Them>(ep_sq);
    state_->cap      = cap;
    state_->fifty_mv = 0;
    break;
  }

  // Update castling rights and keys
  state_->key      ^= Zobrist::CASTLE_KEYS[state_->castling];
  state_->castling &= castling_mask_.rights[src] & castling_mask_.rights[dst];
  state_->key      ^= Zobrist::CASTLE_KEYS[state_->castling];

  // Update board state
  state_->key ^= Zobrist::SIDE_KEY;
  stm_         = ~stm_;

  update_masks<Them>();
}

template <Colour Us>
void Board::undo_move() {
  constexpr Colour Them = ~Us;
  constexpr Piece  King = make_piece(Us, K);
  constexpr Piece  Pawn = make_piece(Us, P);
  const Move       move = state_->mv;
  const Piece      cap  = state_->cap;
  const Square     src  = MoveUtils::src(move);
  const Square     dst  = MoveUtils::dst(move);
  const MoveFlag   flag = MoveUtils::flag(move);

  Square rook_dst, king_dst;
  bool   queen_side;

  switch (flag) {
  case Quiet:
  case DoublePush:  // Move the piece back
    move_piece<false, Us>(dst, src);
    break;
  case Cap:
    move_piece<false, Us>(dst, src);
    set_piece<false, Them>(cap, dst);
    break;
  case KingCastle:
  case QueenCastle:
    queen_side = flag == QueenCastle;
    rook_dst   = castling_mask_.rook_dst<Us>(queen_side);
    king_dst   = castling_mask_.king_dst<Us>(queen_side);
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

  state_--;
  stm_ = ~stm_;
  half_mv_--;
}

template void Board::do_move<White>(Move move);
template void Board::do_move<Black>(Move move);
template void Board::undo_move<White>();
template void Board::undo_move<Black>();

}  // namespace Lyra
