#pragma once

#include "board.hpp"
#include "defs.hpp"
#include "utils.hpp"

namespace Lyra {

using Killer = NDArray<Move, 2>;

struct PieceTo {
  Piece  pc;
  Square to;
};

constexpr PieceTo piece_to(const Board &board, Move move) {
  return {board.on(MoveUtils::src(move)), MoveUtils::dst(move)};
}

constexpr void update_killer(Killer &killer, Move best) {
  if (killer[0] != best) {
    killer[1] = killer[0];
    killer[0] = best;
  }
}

} // namespace Lyra
