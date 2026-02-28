#pragma once

#include <cstdint>

namespace Lyra {

/******************************************\
|==========================================|
|                 Aliases                  |
|==========================================|
\******************************************/

using I8  = int8_t;
using I16 = int16_t;
using I32 = int32_t;
using U8  = uint8_t;
using U16 = uint16_t;
using U64 = uint64_t;

using Key   = U64;
using Ply   = U16;
using Depth = I16;
using Eval  = I32;
using Move  = U16;

/******************************************\
|==========================================|
|               Board Types                |
|==========================================|
\******************************************/

// clang-format off
enum Square : I8 {
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
    NoSquare, NSquare = 64,
};

enum Rank : I8 {
    Rank1, Rank2, Rank3, Rank4, Rank5, Rank6, Rank7, Rank8, NoRank, NRank = 8
};

enum File : I8 {
    FileA, FileB, FileC, FileD, FileE, FileF, FileG, FileH, NoFile, NFile = 8
};

enum Piece : I8 {
    wP, bP, wN, bN, wB, bB, wR, bR, wQ, bQ, wK, bK, NoPiece, NPiece = 12
};

enum PieceType: I8 {
    P, N, B, R, Q, K, NoPieceType, NPieceType = 6,
};

enum Colour: I8 {
    White, Black, Both, NColour = 2,
};

enum Castle : I8 {
    NoCastle = 0b0000,
    WhiteOO = 0b0001,
    WhiteOOO = 0b0010,
    BlackOO = 0b0100,
    BlackOOO = 0b1000,

    BothOO = 0b0101,
    BothOOO = 0b1010,

    AnyCastle = 0b1111,
    NCastling = 16
};

enum class Direction : I8 {
    N = 8, S = -8, E = -1, W = 1,
    NW = 9, NE = 7, SW = -7, SE = -9,
    NoDir = 0
};

enum GamePhase : I8 {
  MidGame,
  Endgame,
  NGamePhase,
};

// clang-format on

/******************************************\
|==========================================|
|            Board Type Helpers            |
|==========================================|
\******************************************/

constexpr Square    operator++(Square &sq) noexcept { return sq = static_cast<Square>(sq + 1); }
constexpr Square    operator--(Square &sq) noexcept { return sq = static_cast<Square>(sq - 1); }
constexpr Rank      operator++(Rank &r) noexcept { return r = static_cast<Rank>(r + 1); }
constexpr Rank      operator--(Rank &r) noexcept { return r = static_cast<Rank>(r - 1); }
constexpr File      operator++(File &f) noexcept { return f = static_cast<File>(f + 1); }
constexpr File      operator--(File &f) noexcept { return f = static_cast<File>(f - 1); }
constexpr PieceType operator++(PieceType &pt) noexcept {
  return pt = static_cast<PieceType>(pt + 1);
}
constexpr Piece  operator++(Piece &pc) noexcept { return pc = static_cast<Piece>(pc + 1); }
constexpr Castle operator++(Castle &cr) noexcept { return cr = static_cast<Castle>(cr + 1); }

constexpr Direction operator~(Direction dir) noexcept { return static_cast<Direction>(-I8(dir)); }
constexpr Colour    operator~(Colour c) noexcept { return static_cast<Colour>(c ^ Black); }
constexpr Castle    operator~(Castle cr) noexcept { return static_cast<Castle>(cr ^ AnyCastle); }
constexpr Castle    operator&(Castle cr1, Castle cr2) noexcept {
  return static_cast<Castle>(I8(cr1) & I8(cr2));
}
constexpr Castle &operator&=(Castle &cr1, Castle cr2) noexcept { return cr1 = cr1 & cr2; }
constexpr Castle  operator|(Castle cr1, Castle cr2) noexcept {
  return static_cast<Castle>(I8(cr1) | I8(cr2));
}
constexpr Castle operator|=(Castle &cr1, Castle cr2) noexcept { return cr1 = cr1 | cr2; }

constexpr Rank   rank_of(Square sq) noexcept { return static_cast<Rank>(sq >> 3); }
constexpr File   file_of(Square sq) noexcept { return static_cast<File>(sq & 7); }
constexpr Square make_square(File f, Rank r) noexcept { return static_cast<Square>(r << 3 | f); }

constexpr PieceType pt_of(Piece pc) noexcept { return static_cast<PieceType>(pc >> 1); }
constexpr Colour    colour_of(Piece pc) noexcept { return static_cast<Colour>(pc & 1); }
constexpr Piece     make_piece(Colour c, PieceType pt) noexcept {
  return static_cast<Piece>(pt << 1 | c);
}

/******************************************\
|==========================================|
|               Eval/Score                 |
|==========================================|
\******************************************/

struct Score {
  Eval mg;
  Eval eg;

  constexpr Eval to_eval(int mg_phase) const {
    if (mg_phase > 24) mg_phase = 24;
    int eg_phase = 24 - mg_phase;
    return (mg * mg_phase + eg * eg_phase) / 24;
  }
};

constexpr Score  operator-(Score s) { return {-s.mg, -s.eg}; }
constexpr Score  operator+(Score s1, Score s2) { return {s1.mg + s2.mg, s1.eg + s2.eg}; }
constexpr Score  operator-(Score s1, Score s2) { return {s1.mg - s2.mg, s1.eg - s2.eg}; }
constexpr Score &operator+=(Score &s1, Score s2) { return s1 = s1 + s2; }
constexpr Score &operator-=(Score &s1, Score s2) { return s1 = s1 - s2; }

/******************************************\
|==========================================|
|              Misc Functions              |
|==========================================|
\******************************************/

constexpr Square flip_rank(Square sq) noexcept { return static_cast<Square>(sq ^ A8); }
constexpr Square relative_sq(Colour c, Square sq) noexcept {
  return c == White ? sq : flip_rank(sq);
}
constexpr Castle relative_castle(Colour c, Castle cr) noexcept {
  return c == White ? cr : Castle(cr << 2);
}

template <Colour C> constexpr Square forward(Square sq) noexcept {
  return static_cast<Square>(C == White ? sq + I8(Direction::N) : sq + I8(Direction::S));
}

template <Direction dir> constexpr Square shift(Square sq) {
  return static_cast<Square>(sq + I8(dir));
}

} // namespace Lyra
