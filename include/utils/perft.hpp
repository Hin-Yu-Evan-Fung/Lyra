#pragma once

#include "board/board.hpp"
#include "core/defs.hpp"

namespace Lyra {

enum class PerftMode {
  Norm,
  MP,
};

void perft(Board &board, Depth depth, PerftMode mode);

struct BenchTestCase {
  std::string fen;
  Depth       depth;
  U64         nodes;
};

bool perft_bench();
bool perft_bench(BenchTestCase test_cases[], int n_cases, PerftMode mode);

} // namespace Lyra
