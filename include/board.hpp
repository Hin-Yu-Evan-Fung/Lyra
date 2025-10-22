#pragma once

#include "bitboard.hpp"
#include "castling.hpp"
#include "defs.hpp"
#include "move_defs.hpp"
#include "utils.hpp"
#include "zobrist.hpp"

namespace Lyra {

/******************************************\
|==========================================|
|                Undo State                |
|==========================================|
\******************************************/

// Undo state, used for do_move and undo_move
struct UndoState {
    Piece  cap;
    Castle castling;
    Square ep;

    I8   fifty_mv;
    Move mv;
    Key  key;
    Key  pawn_key;

    BB   check_mask, diag_pin, hv_pin, attacked;
    bool can_ep;
};

/******************************************\
|==========================================|
|               Board Struct               |
|==========================================|
\******************************************/

struct Board {
private:
    NDArray<BB, NPieceType> pieceBB;
    NDArray<BB, NColour>    colourBB;
    NDArray<Piece, NSquare> board;

    Colour     _stm;
    U16        half_mv;
    CastleMask _castling_mask;

    UndoState* _state;
    UndoState* history;

    template <Colour C>
    constexpr void set_piece(Piece pc, Square sq);
    template <Colour C>
    constexpr void pop_piece(Square sq);
    template <Colour C>
    constexpr void move_piece(Square src, Square dst);

    template <Colour C>
    constexpr void update_masks();
    template <Colour C>
    constexpr BB checkers();

public:
    Board();
    ~Board();
    Board(Board& board) = delete;  // No Copying

    void set(const std::string& fen);
    void reset();

    void        print() const;
    std::string fen() const;

    template <Colour Us>
    constexpr void do_move(Move move);
    template <Colour Us>
    constexpr void undo_move();

    constexpr BB bb() const;
    constexpr BB bb(Colour c) const;
    constexpr BB bb(PieceType pt) const;
    constexpr BB bb(Piece pc) const;
    constexpr BB bb(Piece pc1, Piece pc2) const;

    constexpr Piece on(Square sq) const;
    template <Colour C>
    constexpr Square ksq() const;

    constexpr Colour     stm() const;
    constexpr CastleMask castling_mask() const;

    constexpr UndoState* state();
    constexpr UndoState* state() const;
};

/******************************************\
|==========================================|
|           Board State Helpers            |
|==========================================|
\******************************************/

constexpr UndoState* Board::state() { return _state; }
constexpr UndoState* Board::state() const { return _state; }
constexpr Colour     Board::stm() const { return _stm; }
constexpr CastleMask Board::castling_mask() const { return _castling_mask; }

/******************************************\
|==========================================|
|          Board BitBoard Getters          |
|==========================================|
\******************************************/

constexpr BB Board::bb() const { return colourBB[White] & colourBB[Black]; }
constexpr BB Board::bb(Colour c) const { return colourBB[c]; }
constexpr BB Board::bb(PieceType pt) const { return pieceBB[pt]; }
constexpr BB Board::bb(Piece pc) const { return bb(colour_of(pc)) & bb(pt_of(pc)); }
constexpr BB Board::bb(Piece pc1, Piece pc2) const { return bb(pc1) | bb(pc2); }

/******************************************\
|==========================================|
|              Board Getters               |
|==========================================|
\******************************************/

constexpr Piece Board::on(Square sq) const { return board[sq]; }
template <Colour C>
constexpr Square Board::ksq() const {
    return BBUtils::lsb(bb(make_piece(C, K)));
}

}  // namespace Lyra

#include "move_impl.hpp"
