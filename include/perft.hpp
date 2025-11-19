#pragma once

#include "board.hpp"
#include "defs.hpp"

namespace Lyra {

enum PerftMode {
  Perft,
  Perft_MP,
};

template <PerftMode MP>
void perft(Board& board, Depth d);

void perft_bench();

}  // namespace Lyra
