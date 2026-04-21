#include "move.hpp"

#include "bitboard.hpp"
#include "board.hpp"
#include "defs.hpp"
#include "mask.hpp"
#include "zobrist.hpp"

#include <cstring>

namespace Lyra {

/******************************************\
|==========================================|
|              Do / Undo Move              |
|==========================================|
\******************************************/

void Board::do_move(Move move) {
  return stm_ == White ? do_move<White>(move) : do_move<Black>(move);
}

template <Colour Us>
void Board::do_move(Move move) {
  constexpr Colour Them = ~Us;
  constexpr Piece  King = make_piece(Us, K);
  constexpr Piece  Pawn = make_piece(Us, P);

  const Square   src  = MoveUtils::src(move);
  const Square   dst  = MoveUtils::dst(move);
  const Piece    pc   = on(src);
  const MoveFlag flag = MoveUtils::flag(move);
  Piece          cap  = on(dst);

  Piece  promo;
  Square rook_dst, king_dst, ep;
  bool   queen_side;

  // Initialise new state
  Undo *prev = undo_++;
  std::memcpy(undo_, prev, offsetof(Undo, ep));

  // Update new state
  undo_->key ^= (prev->ep != NoSquare) * Zobrist::EP_KEYS[file_of(prev->ep)];
  undo_->ep   = NoSquare;
  undo_->cap  = cap;
  undo_->move = move;

  // Increment move counters
  undo_->rule50++;
  undo_->ply++;
  gameply_++;

  switch (flag) {
  case Quiet:
    move_piece<true, Us>(src, dst);
    if (pc == Pawn) undo_->rule50 = 0;
    break;
  case Cap:
    pop_piece<true, Them>(dst);
    move_piece<true, Us>(src, dst);
    undo_->rule50 = 0;
    break;
  case DoublePush:
    move_piece<true, Us>(src, dst);
    ep            = forward<Us>(src);
    undo_->rule50 = 0;
    if (!can_ep<Them>(ep)) break;
    undo_->ep = ep;
    undo_->key ^= Zobrist::EP_KEYS[file_of(ep)];
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
    ep  = forward<Them>(dst);
    cap = make_piece(Them, P);
    move_piece<true, Us>(src, dst);
    pop_piece<true, Them>(ep);
    undo_->cap    = cap;
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
}

template <Colour Us>
void Board::undo_move() {
  constexpr Colour Them = ~Us;
  constexpr Piece  King = make_piece(Us, K);
  constexpr Piece  Pawn = make_piece(Us, P);

  const Move     move = undo_->move;
  const Piece    cap  = undo_->cap;
  const Square   src  = MoveUtils::src(move);
  const Square   dst  = MoveUtils::dst(move);
  const MoveFlag flag = MoveUtils::flag(move);

  Square rook_dst, king_dst;
  bool   queen_side;

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

  undo_--;
  stm_ = ~stm_;
  gameply_--;
}

template <Colour Us>
bool Board::is_legal(Move move) const {
  using namespace MoveUtils;
  using namespace BBUtils;

  constexpr Colour Them = ~Us;
  constexpr Castle OO   = Us == White ? WhiteOO : BlackOO;
  constexpr Castle OOO  = Us == White ? WhiteOOO : BlackOOO;

  if (!move) return false;

  // Move params
  const Square    src_       = src(move);
  const Square    dst_       = dst(move);
  const Piece     pc         = on(src_);
  const PieceType pt         = pt_of(pc);
  const Piece     cap        = on(dst_);
  const MoveFlag  flag_      = flag(move);
  const bool      is_castle_ = is_castle(move);
  const bool      is_cap_    = is_capture(move);
  const bool      is_ep_     = flag_ == EP;
  const bool      is_dp_     = flag_ == DoublePush;
  const bool      is_quiet_  = flag_ == Quiet;

  // Board info
  const Square ksq_       = ksq<Us>();
  const Square ep_        = undo_->ep;
  const Square ep_target_ = forward<Them>(ep_);
  const BB     occ        = bb();
  const BB     hv_pin     = undo_->hv_pin;
  const BB     diag_pin   = undo_->diag_pin;
  const BB     check_mask = undo_->check_mask;
  const BB     attacked   = undo_->attacked;

  // Check if the moving piece is ours or not
  const bool invalid_pc = pc == NoPiece || colour_of(pc) != Us;
  // Check if the captured piece is theirs or not (Except for castling where we can capture our own
  // rook)
  const bool invalid_cap = !is_castle_ && cap != NoPiece && colour_of(cap) == Us;
  // Check if the squares makes sense (Chess960 allows for src == dst for castling)
  const bool invalid_sq = !is_castle_ && src_ == dst_;
  // Check if the capture flag is consistent with a piece being captured
  const bool invalid_cap_flag = !is_castle_ && !is_ep_ && is_cap_ == (cap == NoPiece);

  if (invalid_pc || invalid_cap || invalid_sq || invalid_cap_flag) return false;

  if (is_castle_) {
    if (flag_ == KingCastle && undo_->c_rights & OO) return !in_check() && can_castle<Us, false>();
    if (flag_ == QueenCastle && undo_->c_rights & OOO) return !in_check() && can_castle<Us, true>();
    return false;
  }

  // Check if there is a pin and if so check if the movement is aligned to the king
  const bool valid_pin =
      !(diag_pin & from(src_) || hv_pin & from(src_)) || is_aligned(src_, dst_, ksq_);

  if (is_ep_)
    return undo_->ep == dst_ && valid_pin && PAWN_ATK[Us][src_] & from(dst_)
           && from(ep_target_) & check_mask;

  switch (pt) {
  case P:
    // Capture is illegal if its the wrong attack pattern
    if (is_cap_ && !(PAWN_ATK[Us][src_] & from(dst_))) return false;
    // Double Push is illegal if there are pieces in the way
    if (is_dp_ && (BTWN_BB[src_][dst_] | from(dst_)) & occ) return false;
    // Single Push is illegal if its the wrong pattern and there are pieces in the way
    if (is_quiet_ && (dst_ != forward<Us>(src_) || from(dst_) & occ)) return false;
    // Check for pins and checks
    return valid_pin && from(dst_) & check_mask;
  case N: return valid_pin && from(dst_) & check_mask & KNIGHT_ATK[src_];
  case B: return valid_pin && from(dst_) & check_mask & BISHOP_ATK[src_][occ];
  case R: return valid_pin && from(dst_) & check_mask & ROOK_ATK[src_][occ];
  case Q:
    return valid_pin && from(dst_) & check_mask & (BISHOP_ATK[src_][occ] | ROOK_ATK[src_][occ]);
  case K: return KING_ATK[src_] & from(dst_) && !(attacked & from(dst_));
  case NoPieceType: return false;
  }

  return true;
}

template void Board::do_move<White>(Move move);
template void Board::do_move<Black>(Move move);
template void Board::undo_move<White>();
template void Board::undo_move<Black>();
template bool Board::is_legal<White>(Move move) const;
template bool Board::is_legal<Black>(Move move) const;

} // namespace Lyra
