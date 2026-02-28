#include "utils/perft.hpp"

#include <gtest/gtest.h>

namespace Lyra {

BenchTestCase normal[] = {
    {"1k6/1b6/8/8/7R/8/8/4K2R b K - 0 1", 5, 1063513},
    {"3k4/3p4/8/K1P4r/8/8/8/8 b - - 0 1", 6, 1134888},
    {"8/8/4k3/8/2p5/8/B2P2K1/8 w - - 0 1", 6, 1015133},
    {"8/8/1k6/2b5/2pP4/8/5K2/8 b - d3 0 1", 6, 1440467},
    {"5k2/8/8/8/8/8/8/4K2R w K - 0 1", 6, 661072},
    {"3k4/8/8/8/8/8/8/R3K3 w Q - 0 1", 6, 803711},
    {"r3k2r/1b4bq/8/8/8/8/7B/R3K2R w KQkq - 0 1", 4, 1274206},
    {"r3k2r/8/3Q4/8/8/5q2/8/R3K2R b KQkq - 0 1", 4, 1720476},
    {"2K2r2/4P3/8/8/8/8/8/3k4 w - - 0 1", 6, 3821001},
    {"8/8/1P2K3/8/2n5/1q6/8/5k2 b - - 0 1", 5, 1004658},
    {"4k3/1P6/8/8/8/8/K7/8 w - - 0 1", 6, 217342},
    {"8/P1k5/K7/8/8/8/8/8 w - - 0 1", 6, 92683},
    {"K1k5/8/P7/8/8/8/8/8 w - - 0 1", 6, 2217},
    {"8/k1P5/8/1K6/8/8/8/8 w - - 0 1", 7, 567584},
    {"8/8/2k5/5q2/5n2/8/5K2/8 b - - 0 1", 4, 23527},
    {"4k3/8/8/8/8/8/8/4K2R w K - 0 1 ", 6, 764643},
    {"4k3/8/8/8/8/8/8/R3K3 w Q - 0 1 ", 6, 846648},
    {"4k2r/8/8/8/8/8/8/4K3 w k - 0 1 ", 6, 899442},
    {"r3k3/8/8/8/8/8/8/4K3 w q - 0 1 ", 6, 1001523},
    {"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 5, 674624},
};

TEST(perft, bench_normal) { ASSERT_TRUE(perft_bench(normal, 20, PerftMode::Norm)); };
TEST(perft, bench_mp_normal) { ASSERT_TRUE(perft_bench(normal, 20, PerftMode::MP)); };

BenchTestCase chess960[] = {
    {"8/3k4/8/8/8/8/8/rR2K3 w Q - 0 1", 6, 7137508},
    {"Rr2k3/8/8/8/8/8/8/rR2K3 w Qq - 0 1", 6, 46081241},
    {"2k5/8/8/8/b7/8/8/2K3R1 w - - 0 1", 6, 6578528},
    {"3k4/8/8/8/8/8/8/rRK5 w - - 0 1", 6, 3191684},
};

TEST(perft, bench_chess960) { ASSERT_TRUE(perft_bench(chess960, 4, PerftMode::Norm)); };
TEST(perft, bench_mp_chess960) { ASSERT_TRUE(perft_bench(chess960, 4, PerftMode::MP)); };

} // namespace Lyra
