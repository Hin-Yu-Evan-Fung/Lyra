#include "board.hpp"
#include "defs.hpp"
#include "move.hpp"

#include <gtest/gtest.h>

namespace Lyra {

TEST(gives_check, direct) {
  Board board;
  board.set("r1bqkbnr/pppp1ppp/2n5/4p3/3PP3/5N2/PPP2PPP/RNBQKB1R b KQkq - 0 3");
  ASSERT_TRUE(board.gives_check<Black>(MoveUtils::encode<Quiet>(F8, B4)));
  ASSERT_FALSE(board.gives_check<Black>(MoveUtils::encode<Quiet>(F8, D6)));
}

TEST(gives_check, direct_2) {
  Board board;
  board.set("r1bqkbnr/pppp1ppp/2n5/4p3/3PP3/2P2N2/PP2KPPP/RNBQ1B1R b kq - 2 5");
  ASSERT_TRUE(board.gives_check<Black>(MoveUtils::encode<Cap>(C6, D4)));
  ASSERT_FALSE(board.gives_check<Black>(MoveUtils::encode<Cap>(C6, B4)));
}

TEST(gives_check, discovered) {
  Board board;
  board.set("1R6/8/1K6/8/8/8/8/1k6 w - - 0 1");
  ASSERT_TRUE(board.gives_check<White>(MoveUtils::encode<Quiet>(B6, C5)));
  ASSERT_FALSE(board.gives_check<White>(MoveUtils::encode<Quiet>(B6, B5)));
}

TEST(gives_check, discovered_2) {
  Board board;
  board.set("8/7B/8/8/8/3K4/8/1k6 w - - 0 1");
  ASSERT_TRUE(board.gives_check<White>(MoveUtils::encode<Quiet>(D3, C3)));
  ASSERT_FALSE(board.gives_check<White>(MoveUtils::encode<Quiet>(D3, E4)));
}

TEST(gives_check, castle) {
  Board board;
  board.set("3k4/8/8/8/8/8/8/R3K3 w Q - 0 1");
  ASSERT_TRUE(board.gives_check<White>(MoveUtils::encode<QueenCastle>(E1, A1)));
  ASSERT_FALSE(board.gives_check<White>(MoveUtils::encode<Quiet>(A1, C1)));
}

TEST(gives_check, ep) {
  Board board;
  board.set("6k1/8/8/8/K2Pp2r/8/8/8 b - d3 0 1");
  ASSERT_TRUE(board.gives_check<Black>(MoveUtils::encode<EP>(E4, D3)));
  ASSERT_FALSE(board.gives_check<Black>(MoveUtils::encode<Quiet>(E4, E3)));
}

} // namespace Lyra
