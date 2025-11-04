#pragma once

#include <immintrin.h>

#include "defs.hpp"
#include "immintrin.h"
#include "utils.hpp"

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
inline int    count(BB bb) { return _mm_popcnt_u64(bb); }
constexpr int more_than_one(BB bb) { return (bb & (bb - 1)) != 0; }

template <typename Func>
constexpr void loop(BB bb, Func&& func) {
    for (; bb; bb &= bb - 1)
        func(lsb(bb));
}

/******************************************\
|==========================================|
|               Shift Helpers              |
|==========================================|
\******************************************/

template <Direction D>
constexpr BB shift(BB b) {
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

template <Direction D1, Direction D2, Direction... Ds>
constexpr BB shift(BB b) {
    return shift<D1>(b) | shift<D2, Ds...>(b);
}

/******************************************\
|==========================================|
|              Lookup Tables               |
|==========================================|
\******************************************/

struct Magic {
    BB* attacks;
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

extern NDArray<BB, NColour, NSquare> PAWN_ATK;
extern NDArray<BB, NSquare>          KNIGHT_ATK;
extern NDArray<BB, NSquare>          KING_ATK;
extern NDArray<Magic, NSquare>       BISHOP_ATK;
extern NDArray<Magic, NSquare>       ROOK_ATK;
extern NDArray<BB, NSquare, NSquare> BTWN_BB;
extern NDArray<BB, NSquare, NSquare> CHECK_BB;

template <Colour C>
constexpr BB pawn_attack(BB bb) {
    using enum Direction;
    return C == White ? shift<NE, NW>(bb) : shift<SE, SW>(bb);
}

}  // namespace BBUtils

}  // namespace Lyra
