#include "board.hpp"

#include <cassert>
#include <ios>
#include <iostream>
#include <sstream>

#include "bitboard.hpp"
#include "castling.hpp"
#include "defs.hpp"
#include "mask.hpp"
#include "utils.hpp"
#include "zobrist.hpp"

namespace Lyra {

/******************************************\
|==========================================|
|              Input/Output                |
|==========================================|
\******************************************/

Board::Board() {
    _history = new UndoState[MAX_DEPTH];
    reset();
}

Board::~Board() { delete[] _history; }

void Board::reset() {
    _half_mv         = 0;
    _state           = _history;
    _state->castling = NoCastle;
    _state->ep       = NoSquare;
    _state->fifty_mv = 0;

    pieceBB.fill(BBUtils::EmptyBB);
    colourBB.fill(BBUtils::EmptyBB);
    board.fill(NoPiece);
    _castling_mask.reset();
}

void Board::set(const std::string& fen) {
    std::istringstream ss(fen);
    std::string        part;

    int    file = FileA;
    int    rank = Rank8;
    Square sq;

    reset();

    // 1. Parse pieces
    ss >> std::skipws >> part;
    for (char c : part) {
        sq = make_square(File(file), Rank(rank));

        if (std::isalpha(c)) {
            Piece pc = char2piece(c);
            if (colour_of(pc) == White)
                set_piece<White>(pc, sq);
            else
                set_piece<Black>(pc, sq);
            file++;
        } else if (std::isdigit(c)) {
            file += c - '0';
        } else if (c == '/') {
            file = 0;
            rank--;
        }
    }

    // 2. Parse side to move
    ss >> std::skipws >> part;
    _stm = part[0] == 'w' ? White : Black;

    // 3. Parse castling
    ss >> std::skipws >> part;
    Castle castling = NoCastle;
    for (char c : part) {
        Colour s     = std::isupper(c) ? White : Black;
        char   upper = std::toupper(c);

        Piece  rook  = make_piece(s, R);
        Square ksq   = s == White ? Board::ksq<White>() : Board::ksq<Black>();
        Square rsq;

        if (upper == 'K') {
            rsq = relative(s, H1);
            while (on(rsq) != rook)
                --rsq;
            castling = CastleMask::get_mask(s, false);
            _castling_mask.add_rights(ksq, rsq, castling);
        } else if (upper == 'Q') {
            rsq = relative(s, A1);
            while (on(rsq) != rook)
                ++rsq;
            castling = CastleMask::get_mask(s, true);
            _castling_mask.add_rights(ksq, rsq, castling);
        } else if (upper >= 'A' && upper <= 'H') {
            rsq      = relative(s, make_square(char2file(upper), Rank1));
            castling = CastleMask::get_mask(s, ksq > rsq);
            _castling_mask.add_rights(ksq, rsq, castling);
        }

        _state->castling |= castling;
    }

    // 4. Parse enpassant
    ss >> std::skipws >> part;
    _state->ep = NoSquare;
    if (part.length() == 2) { _state->ep = str2sq(part); }

    int fifty_mv, full_mv;
    ss >> std::skipws >> fifty_mv;
    ss >> std::skipws >> full_mv;

    _state->fifty_mv = I8(fifty_mv);
    _half_mv         = I8(full_mv - 1) * 2 + I8(_stm);

    _state->key      = compute_key();

    // Basic board legality checks
    if (ksq<White>() == NoSquare) throw std::invalid_argument("Invalid fen! White king is not on the board!");
    if (ksq<Black>() == NoSquare) throw std::invalid_argument("Invalid fen! Black king is not on the board!");

    _stm == White ? update_masks<White>() : update_masks<Black>();
}

void Board::print() const {
    std::string sep{"\n     +---+---+---+---+---+---+---+---+\n"};
    std::cout << sep;

    for (Rank r = Rank8; r >= Rank1; --r) {
        std::cout << " " << to_str(r) << "   |";
        for (File f = FileA; f <= FileH; ++f) {
            Square sq = make_square(f, r);

            std::cout << " " << to_str(on(sq)) << " " << "|";
        }
        std::cout << sep;
    }
    std::cout << "\n";
    std::cout << "       A   B   C   D   E   F   G   H\n\n";
    std::cout << "Fen: " << fen() << "\n";
    std::cout << "Side to move: " << (_stm == Colour::White ? "White" : "Black") << "\n";
    std::cout << "Castling rights: " << _castling_mask.to_str(_state->castling) << "\n";
    std::cout << "Enpassant square: " << to_str(_state->ep) << "\n";
    std::cout << "Hash key: " << std::hex << _state->key << std::dec << "\n";
}

std::string Board::fen() const {
    std::ostringstream out;
    Square             sq;
    Piece              pc;

    for (Rank r = Rank8; r >= Rank1; --r) {
        int empty_count = 0;
        for (File f = FileA; f <= FileH; ++f) {
            sq = make_square(f, r);
            pc = on(sq);
            if (pc != NoPiece) {
                if (empty_count != 0) out << empty_count;
                out << to_str(pc);
                empty_count = 0;
            } else {
                empty_count += 1;
            }
        }

        if (empty_count != 0) out << empty_count;
        if (r != Rank1)  // Move the piece back
            out << '/';
    }

    out << " " << (_stm == Colour::White ? "w" : "b");
    out << " " << _castling_mask.to_str(_state->castling);
    out << " " << (_state->ep != NoSquare ? to_str(_state->ep) : "-");
    out << " " << int(_state->fifty_mv);
    out << " " << _half_mv / 2 + 1;

    return out.str();
}

/******************************************\
|==========================================|
|                 Hashing                  |
|==========================================|
\******************************************/

Zobrist::Key Board::compute_key() const {
    Zobrist::Key key = 0;

    for (Square sq = A1; sq <= H8; ++sq) {
        Piece pc = on(sq);
        if (pc != NoPiece) key ^= Zobrist::PIECE_KEYS[sq][pc];
    }

    if (_stm == Black) key ^= Zobrist::SIDE_KEY;
    if (_state->ep != NoSquare) key ^= Zobrist::EP_KEYS[_state->ep];

    key ^= Zobrist::CASTLE_KEYS[_state->castling];

    return key;
}

}  // namespace Lyra
