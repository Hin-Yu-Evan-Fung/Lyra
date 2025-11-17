#pragma once

#include "board.hpp"
#include "defs.hpp"

namespace Lyra {

void perft(Board& board, Depth d);

void perft_bench();

}  // namespace Lyra
