#pragma once

#include "bitboard.hpp"
#include "castling.hpp"
#include "defs.hpp"
#include "move.hpp"
#include "utils.hpp"
#include "zobrist.hpp"

namespace Lyra {

/******************************************\
|==========================================|
|            Useful fen strings            |
|==========================================|
\******************************************/

constexpr std::string_view start_pos = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 ";

/******************************************\
|==========================================|
|                Undo State                |
|==========================================|
\******************************************/

// Undo state, used for do_move and undo_move
struct alignas(64) UndoState {
    Piece  cap;
    Castle castling;
    Square ep;

    I8           fifty_mv;
    Move         mv;
    Zobrist::Key key;
    Zobrist::Key pawn_key;

    BB   check_mask, diag_pin, hv_pin, attacked;
    bool ep_pin;
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
    U16        _half_mv;
    CastleMask _castling_mask;

    UndoState* _state;
    UndoState* _history;

    template <Colour C>
    constexpr void set_piece(Piece pc, Square sq);
    template <Colour C>
    constexpr void pop_piece(Square sq);
    template <Colour C>
    constexpr void move_piece(Square src, Square dst);

    template <Colour Us>
    constexpr void update_masks();
    template <Colour Us, bool inCheck>
    constexpr void update_pin_and_check_masks();
    template <Colour Us>
    constexpr void update_ep_pin();
    template <Colour Us>
    constexpr BB checkers();
    template <Colour Us>
    constexpr BB threatened();

public:
    Board();
    ~Board();
    Board(Board& board) = delete;  // No Copying

    void set(const std::string& fen);
    void reset();

    void        print() const;
    std::string fen() const;

    void do_move(Move move);
    template <Colour Us>
    void do_move(Move move);
    template <Colour Us>
    void undo_move();

    Zobrist::Key compute_key() const;
    Zobrist::Key compute_pawn_key() const;

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

constexpr BB Board::bb() const { return colourBB[White] | colourBB[Black]; }
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

/******************************************\
|==========================================|
|            Piece Manipulation            |
|==========================================|
\******************************************/

template <Colour C>
constexpr void Board::set_piece(Piece pc, Square sq) {
    colourBB[C]        |= BBUtils::from(sq);
    pieceBB[pt_of(pc)] |= BBUtils::from(sq);
    board[sq]           = pc;
}

template <Colour C>
constexpr void Board::pop_piece(Square sq) {
    const Piece pc      = board[sq];
    colourBB[C]        &= ~BBUtils::from(sq);
    pieceBB[pt_of(pc)] &= ~BBUtils::from(sq);
    board[sq]           = NoPiece;
}

template <Colour C>
constexpr void Board::move_piece(Square src, Square dst) {
    const Piece pc      = board[src];
    colourBB[C]        ^= BBUtils::from(src) ^ BBUtils::from(dst);
    pieceBB[pt_of(pc)] ^= BBUtils::from(src) ^ BBUtils::from(dst);
    board[src]          = NoPiece;
    board[dst]          = pc;
}

}  // namespace Lyra
