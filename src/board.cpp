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

Board::Board() { reset(); }

void Board::reset() {
    half_mv_         = 0;
    state_           = history_.data();
    state_->castling = NoCastle;
    state_->ep       = NoSquare;
    state_->fifty_mv = 0;

    pieceBB_.fill(BBUtils::EmptyBB);
    colourBB_.fill(BBUtils::EmptyBB);
    board_.fill(NoPiece);
    castling_mask_.reset();
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
    stm_ = part[0] == 'w' ? White : Black;

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
            castling_mask_.add_rights(ksq, rsq, castling);
        } else if (upper == 'Q') {
            rsq = relative(s, A1);
            while (on(rsq) != rook)
                ++rsq;
            castling = CastleMask::get_mask(s, true);
            castling_mask_.add_rights(ksq, rsq, castling);
        } else if (upper >= 'A' && upper <= 'H') {
            rsq      = relative(s, make_square(char2file(upper), Rank1));
            castling = CastleMask::get_mask(s, ksq > rsq);
            castling_mask_.add_rights(ksq, rsq, castling);
        }

        state_->castling |= castling;
    }

    // 4. Parse enpassant
    ss >> std::skipws >> part;
    state_->ep = NoSquare;
    if (part.length() == 2) { state_->ep = str2sq(part); }

    int fifty_mv, full_mv;
    ss >> std::skipws >> fifty_mv;
    ss >> std::skipws >> full_mv;

    state_->fifty_mv = I8(fifty_mv);
    half_mv_         = I8(full_mv - 1) * 2 + I8(stm_);

    state_->key      = compute_key();

    // Basic board legality checks
    if (ksq<White>() == NoSquare) throw std::invalid_argument("Invalid fen! White king is not on the board!");
    if (ksq<Black>() == NoSquare) throw std::invalid_argument("Invalid fen! Black king is not on the board!");

    stm_ == White ? update_masks<White>() : update_masks<Black>();
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
    std::cout << "Side to move: " << (stm_ == Colour::White ? "White" : "Black") << "\n";
    std::cout << "Castling rights: " << castling_mask_.to_str(state_->castling) << "\n";
    std::cout << "Enpassant square: " << to_str(state_->ep) << "\n";
    std::cout << "Hash key: " << std::hex << state_->key << std::dec << "\n";
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

    out << " " << (stm_ == Colour::White ? "w" : "b");
    out << " " << castling_mask_.to_str(state_->castling);
    out << " " << (state_->ep != NoSquare ? to_str(state_->ep) : "-");
    out << " " << int(state_->fifty_mv);
    out << " " << half_mv_ / 2 + 1;

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

    if (stm_ == Black) key ^= Zobrist::SIDE_KEY;
    if (state_->ep != NoSquare) key ^= Zobrist::EP_KEYS[state_->ep];

    key ^= Zobrist::CASTLE_KEYS[state_->castling];

    return key;
}

}  // namespace Lyra
