#pragma once

#include "board.hpp"
#include "defs.hpp"

namespace Lyra {

enum class PerftMode {
  Normal,
  MovePick,
};

struct BenchTestCase {
  std::string fen;
  Depth       depth;
  U64         nodes;
};

void perft(PerftMode perft_mode, Board &board, Depth depth);
bool perft_bench(PerftMode perft_mode, BenchTestCase test_cases[], int n_cases);
bool perft_bench();

void run_bench(int argc, char **argv);

} // namespace Lyra
