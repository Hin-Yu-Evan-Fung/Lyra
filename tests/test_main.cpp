#include <gtest/gtest.h>

#include "bitboard.hpp"
#include "zobrist.hpp"

using namespace Lyra;

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  BBUtils::init();
  Zobrist::init();

  return RUN_ALL_TESTS();
}
