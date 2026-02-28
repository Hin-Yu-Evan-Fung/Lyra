#pragma once

#include "defs.hpp"
#include "immintrin.h"

#include <immintrin.h>

namespace Lyra {

/******************************************\
|==========================================|
|            Bitboard Definition           |
|==========================================|
\******************************************/

using BB = uint64_t;

/******************************************\
|==========================================|
|            Bitboard Functions            |
|==========================================|
\******************************************/

namespace BBUtils {

void print(BB bb);
void init();

/******************************************\
|==========================================|
|         Constants and Helpers            |
|==========================================|
\******************************************/

constexpr BB EmptyBB = 0ULL;
constexpr BB FullBB  = ~EmptyBB;
constexpr BB Rank1BB = 0xFFULL;
constexpr BB FileABB = 0x0101010101010101ULL;

constexpr BB from(Square sq) { return 1ULL << sq; }
constexpr BB from(Rank r) { return Rank1BB << (r * 8); }
constexpr BB from(File f) { return FileABB << f; }

#ifdef USE_PEXT
inline U64 pext(BB bb, BB mask) { return _pext_u64(bb, mask); }
#endif
inline Square lsb(BB bb) { return static_cast<Square>(_tzcnt_u64(bb)); }
inline int    popcount(BB bb) { return _mm_popcnt_u64(bb); }
constexpr int more_than_one(BB bb) { return (bb & (bb - 1)) != 0; }

template <typename Func> constexpr void bitloop(BB bb, Func &&func) {
  for (; bb; bb &= bb - 1) func(lsb(bb));
}

/******************************************\
|==========================================|
|               Shift Helpers              |
|==========================================|
\******************************************/

template <Direction D> constexpr BB shift(BB b) {
  using enum Direction;
  switch (D) {
  case N: return b << 8;
  case S: return b >> 8;
  case E: return (b & ~from(FileH)) << 1;
  case W: return (b & ~from(FileA)) >> 1;
  case NE: return (b & ~from(FileH)) << 9;
  case NW: return (b & ~from(FileA)) << 7;
  case SE: return (b & ~from(FileH)) >> 7;
  case SW: return (b & ~from(FileA)) >> 9;
  }
}

template <Direction D1, Direction D2, Direction... Ds> constexpr BB shift(BB b) {
  return shift<D1>(b) | shift<D2, Ds...>(b);
}

/******************************************\
|==========================================|
|              Lookup Tables               |
|==========================================|
\******************************************/

struct Magic {
  BB *attacks;
  BB  mask;
  BB  operator[](BB occ) { return attacks[index(occ)]; }
#ifdef USE_PEXT
  U64 index(BB occ) { return pext(occ, mask); }
#else
  U64 magic;
  int shift;
  U64 index(BB occ) { return ((occ & mask) * magic) >> shift; }
#endif
};

extern BB    PAWN_ATK[NColour][NSquare];
extern BB    KNIGHT_ATK[NSquare];
extern BB    KING_ATK[NSquare];
extern Magic BISHOP_ATK[NSquare];
extern Magic ROOK_ATK[NSquare];
extern BB    LINE_BB[NSquare][NSquare];
extern BB    BTWN_BB[NSquare][NSquare];

/******************************************\
|==========================================|
|               Misc Helpers               |
|==========================================|
\******************************************/

template <Colour C> constexpr BB pawn_attack_bb(BB bb) {
  using enum Direction;
  return C == White ? shift<NE, NW>(bb) : shift<SE, SW>(bb);
}

template <Colour C> constexpr BB attack_bb(PieceType att, Square sq, BB occ) {
  switch (att) {
  case P: return PAWN_ATK[C][sq];
  case N: return KNIGHT_ATK[sq];
  case B: return BISHOP_ATK[sq][occ];
  case R: return ROOK_ATK[sq][occ];
  case Q: return (BISHOP_ATK[sq][occ] | ROOK_ATK[sq][occ]);
  case K: return KING_ATK[sq];
  default: return EmptyBB;
  }
}

constexpr bool is_aligned(Square sq1, Square sq2, Square sq3) {
  return LINE_BB[sq1][sq2] & from(sq3);
}

} // namespace BBUtils

} // namespace Lyra
