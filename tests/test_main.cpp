#include <gtest/gtest.h>

#include "core/bitboard.hpp"
#include "core/zobrist.hpp"

using namespace Lyra;

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  BBUtils::init();
  Zobrist::init();

  return RUN_ALL_TESTS();
}
