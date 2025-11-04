#pragma once

#include "board.hpp"
#include "defs.hpp"

namespace Lyra {

template <bool Div, Colour Us>
U64 perft(Board& board, Depth depth);

void perft_bench();

}  // namespace Lyra
