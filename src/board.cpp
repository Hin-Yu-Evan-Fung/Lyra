#include "board.hpp"

#include <ios>
#include <iostream>
#include <sstream>

#include "bitboard.hpp"
#include "castling.hpp"
#include "defs.hpp"
#include "utils.hpp"
#include "zobrist.hpp"

namespace Lyra {

/******************************************\
|==========================================|
|              Input/Output                |
|==========================================|
\******************************************/

Board::Board() {
    history = new UndoState[MAX_DEPTH];
    reset();
}

Board::~Board() { delete[] history; }

void Board::reset() {
    half_mv          = 0;
    _state           = history;
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

    if (_stm == Black)
        _state->key ^= Zobrist::SIDE_KEY;

    // 3. Parse castling
    ss >> std::skipws >> part;
    Castle castling = NoCastle;
    for (char c : part) {
        Colour s       = std::isupper(c) ? White : Black;
        char   upper   = std::toupper(c);

        Piece  rook    = make_piece(s, R);
        Square king_sq = s == White ? ksq<White>() : ksq<Black>();
        Square rsq;

        if (upper == 'K') {
            rsq = relative(s, H1);
            while (on(rsq) != rook)
                --rsq;
            castling = CastleMask::get_mask(s, false);
            _castling_mask.add_rights(king_sq, rsq, castling);
        } else if (upper == 'Q') {
            rsq = relative(s, A1);
            while (on(rsq) != rook)
                ++rsq;
            castling = CastleMask::get_mask(s, true);
            _castling_mask.add_rights(king_sq, rsq, castling);
        } else if (upper >= 'A' || upper <= 'H') {
            rsq      = relative(s, make_square(char2file(upper), Rank1));
            castling = CastleMask::get_mask(s, king_sq > rsq);
            _castling_mask.add_rights(king_sq, rsq, castling);
        }

        _state->castling |= castling;
    }

    // 4. Parse enpassant
    ss >> std::skipws >> part;
    _state->ep = NoSquare;
    if (part.length() == 2) {
        _state->ep   = str2sq(part);
        _state->key ^= Zobrist::EP_KEYS[file_of(_state->ep)];
    }

    I8 fifty_mv, full_mv;
    ss >> std::skipws >> fifty_mv;
    ss >> std::skipws >> full_mv;
    _state->fifty_mv = I8(fifty_mv);
    half_mv          = (full_mv - 1) * 2 + I8(_stm);
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
    // std::cout << "Fen: " << fen() << "\n"/* ; */
    std::cout << "Side to move: " << (_stm == Colour::White ? "White" : "Black") << "\n";
    std::cout << "Castling rights: " << _castling_mask.to_str(_state->castling) << "\n";
    std::cout << "Enpassant square: " << to_str(_state->ep) << "\n";
    std::cout << "Hash key: " << std::hex << _state->key << std::dec << "\n";
}

}  // namespace Lyra
