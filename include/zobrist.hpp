#pragma once

#include "defs.hpp"
#include "utils.hpp"

namespace Lyra::Zobrist {

inline Key PIECE_KEYS[NPiece][NSquare];
inline Key SIDE_KEY;
inline Key CASTLE_KEYS[NCastling];
inline Key EP_KEYS[NFile];

inline void init() {
  PRNG prng;

  for (Square sq = A1; sq <= H8; ++sq)
    for (Piece pc = wP; pc <= bK; ++pc)
      PIECE_KEYS[pc][sq] = prng.random();

  SIDE_KEY = prng.random();

  for (Castle cr = NoCastle; cr <= AnyCastle; ++cr)
    CASTLE_KEYS[cr] = prng.random();
  for (File f = FileA; f <= FileH; ++f)
    EP_KEYS[f] = prng.random();
}

} // namespace Lyra::Zobrist
