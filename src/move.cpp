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

void Board::do_move(Move move) { return _stm == White ? do_move<White>(move) : do_move<Black>(move); }

template <Colour Us>
void Board::do_move(Move move) {
    constexpr Colour Them = ~Us;
    constexpr Piece  King = make_piece(Us, K);
    constexpr Piece  Rook = make_piece(Us, R);
    constexpr Piece  Pawn = make_piece(Us, P);
    const Square     src  = MoveUtils::src(move);
    const Square     dst  = MoveUtils::dst(move);
    const Piece      pc   = on(src);
    const MoveFlag   flag = MoveUtils::flag(move);
    Piece            promo;
    Square           rook_dst, king_dst, ep_sq;
    bool             queen_side;

    _half_mv++;

    // Initialise new state
    UndoState* prev  = _state++;
    _state->mv       = move;
    _state->castling = prev->castling;
    _state->fifty_mv = prev->fifty_mv + 1;
    //
    Piece cap        = on(dst);
    _state->cap      = cap;
    Zobrist::Key key = prev->key;
    // Update key if enpassant is reset
    key        ^= (prev->ep != NoSquare) * Zobrist::EP_KEYS[file_of(prev->ep)];
    _state->ep  = NoSquare;

    switch (flag) {
    case Quiet:
        move_piece<Us>(src, dst);
        key ^= Zobrist::PIECE_KEYS[src][pc];
        key ^= Zobrist::PIECE_KEYS[dst][pc];
        if (pc == Pawn) _state->fifty_mv = 0;
        break;
    case Cap:
        pop_piece<Them>(dst);
        move_piece<Us>(src, dst);
        key              ^= Zobrist::PIECE_KEYS[dst][cap];
        key              ^= Zobrist::PIECE_KEYS[src][pc];
        key              ^= Zobrist::PIECE_KEYS[dst][pc];
        _state->fifty_mv  = 0;
        break;
    case DoublePush:
        move_piece<Us>(src, dst);
        key              ^= Zobrist::PIECE_KEYS[src][pc];
        key              ^= Zobrist::PIECE_KEYS[dst][pc];
        _state->ep        = forward<Us>(src);
        key              ^= Zobrist::EP_KEYS[file_of(_state->ep)];
        _state->fifty_mv  = 0;
        break;
    case KingCastle:
    case QueenCastle:
        queen_side = flag == QueenCastle;
        rook_dst   = _castling_mask.rook_dst<Us>(queen_side);
        king_dst   = _castling_mask.king_dst<Us>(queen_side);
        pop_piece<Us>(src);
        move_piece<Us>(dst, rook_dst);
        set_piece<Us>(King, king_dst);
        key ^= Zobrist::PIECE_KEYS[src][King];
        key ^= Zobrist::PIECE_KEYS[king_dst][King];
        key ^= Zobrist::PIECE_KEYS[dst][Rook];
        key ^= Zobrist::PIECE_KEYS[rook_dst][Rook];
        break;
    case PromoCap_Q:
    case PromoCap_R:
    case PromoCap_B:
    case PromoCap_N:
        pop_piece<Them>(dst);
        key ^= Zobrist::PIECE_KEYS[dst][cap];
        [[fallthrough]];
    case Promo_Q:
    case Promo_R:
    case Promo_B:
    case Promo_N:
        promo = make_piece(Us, MoveUtils::promoted_pt(move));
        pop_piece<Us>(src);
        set_piece<Us>(promo, dst);
        key              ^= Zobrist::PIECE_KEYS[promo][dst];
        key              ^= Zobrist::PIECE_KEYS[src][pc];
        _state->fifty_mv  = 0;
        break;
    case EP:
        ep_sq = forward<Them>(dst);
        cap   = make_piece(Them, P);
        move_piece<Us>(src, dst);
        pop_piece<Them>(ep_sq);
        key              ^= Zobrist::PIECE_KEYS[src][pc];
        key              ^= Zobrist::PIECE_KEYS[dst][pc];
        key              ^= Zobrist::PIECE_KEYS[ep_sq][cap];
        _state->cap       = cap;
        _state->fifty_mv  = 0;
        break;
    }

    // Update castling rights and keys
    key              ^= Zobrist::CASTLE_KEYS[_state->castling];
    _state->castling &= _castling_mask.rights[src] & _castling_mask.rights[dst];
    key              ^= Zobrist::CASTLE_KEYS[_state->castling];

    // Update side to move and keys
    _state->key = key ^ Zobrist::SIDE_KEY;
    _stm        = ~_stm;

    update_masks<Them>();
}

template <Colour Us>
void Board::undo_move() {
    constexpr Colour Them = ~Us;
    constexpr Piece  King = make_piece(Us, K);
    constexpr Piece  Pawn = make_piece(Us, P);
    const Move       move = _state->mv;
    const Piece      cap  = _state->cap;
    const Square     src  = MoveUtils::src(move);
    const Square     dst  = MoveUtils::dst(move);
    const MoveFlag   flag = MoveUtils::flag(move);

    Square rook_dst, king_dst;
    bool   queen_side;

    switch (flag) {
    case Quiet:
    case DoublePush:  // Move the piece back
        move_piece<Us>(dst, src);
        break;
    case Cap:
        move_piece<Us>(dst, src);
        set_piece<Them>(cap, dst);
        break;
    case KingCastle:
    case QueenCastle:
        queen_side = flag == QueenCastle;
        rook_dst   = _castling_mask.rook_dst<Us>(queen_side);
        king_dst   = _castling_mask.king_dst<Us>(queen_side);
        pop_piece<Us>(king_dst);
        move_piece<Us>(rook_dst, dst);
        set_piece<Us>(King, src);
        break;
    case Promo_N:
    case Promo_B:
    case Promo_R:
    case Promo_Q:
        pop_piece<Us>(dst);
        set_piece<Us>(Pawn, src);
        break;
    case PromoCap_N:
    case PromoCap_B:
    case PromoCap_R:
    case PromoCap_Q:
        pop_piece<Us>(dst);
        set_piece<Us>(Pawn, src);
        set_piece<Them>(cap, dst);
        break;
    case EP:
        move_piece<Us>(dst, src);
        set_piece<Them>(cap, forward<Them>(dst));
        break;
    }

    _state--;
    _stm = ~_stm;
    _half_mv--;
}

template void Board::do_move<White>(Move move);
template void Board::do_move<Black>(Move move);
template void Board::undo_move<White>();
template void Board::undo_move<Black>();

}  // namespace Lyra
