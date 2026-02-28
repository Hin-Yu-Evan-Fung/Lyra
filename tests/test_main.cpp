#include "core/bitboard.hpp"
#include "core/zobrist.hpp"

#include <gtest/gtest.h>

using namespace Lyra;

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  BBUtils::init();
  Zobrist::init();

  return RUN_ALL_TESTS();
}
