#pragma once

#include "defs.hpp"
#include "utils.hpp"

namespace Lyra::Zobrist {

static NDArray<Key, NSquare, NPiece> PIECE_KEYS;
static Key                           SIDE_KEY;
static NDArray<Key, NCastling>       CASTLE_KEYS;
static NDArray<Key, NFile>           EP_KEYS;

inline void init() {
    PRNG prng;

    for (Square sq = A1; sq <= H8; ++sq)
        for (Piece pc = wP; pc <= bK; ++pc)
            PIECE_KEYS[sq][pc] = prng.random();

    SIDE_KEY = prng.random();

    for (Castle cr = NoCastle; cr <= AnyCastle; ++cr)
        CASTLE_KEYS[cr] = prng.random();
    for (File f = FileA; f <= FileH; ++f)
        EP_KEYS[f] = prng.random();
}

}  // namespace Lyra::Zobrist
