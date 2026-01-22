#include <gtest/gtest.h>

#include "board.hpp"
#include "move.hpp"

namespace Lyra {

TEST(is_legal, regular) {
  Board board;
  board.set("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
  ASSERT_FALSE(board.is_legal<White>(MoveUtils::encode<Quiet>(E2, A6)));      // Wrong Flag
  ASSERT_TRUE(board.is_legal<White>(MoveUtils::encode<Cap>(E2, A6)));         // Legal capture
  ASSERT_FALSE(board.is_legal<White>(MoveUtils::encode<Quiet>(A2, A4)));      // Wrong Flag
  ASSERT_TRUE(board.is_legal<White>(MoveUtils::encode<DoublePush>(A2, A4)));  // Legal DoublePush
}

TEST(is_legal, pin) {
  Board board;
  board.set("r3k2r/p1pq1pb1/bn2pnp1/1B1PN3/1p2P3/2N2Q1p/PPPB1PPP/R3K2R b KQkq - 0 1");
  ASSERT_FALSE(board.is_legal<Black>(MoveUtils::encode<Quiet>(D7, C8)));  // Pinned
  ASSERT_TRUE(board.is_legal<Black>(MoveUtils::encode<Cap>(D7, B5)));     // Legal capture
  ASSERT_FALSE(board.is_legal<Black>(MoveUtils::encode<Quiet>(D7, B5)));  // Wrong Flag
}

TEST(is_legal, enpassant_pin) {
  Board board;
  board.set("8/2p5/3p4/KP5r/1R2Pp1k/8/6P1/8 b - - 0 1");
  ASSERT_FALSE(board.is_legal<Black>(MoveUtils::encode<EP>(F4, E3)));    // EP pinned
  ASSERT_TRUE(board.is_legal<Black>(MoveUtils::encode<Quiet>(F4, F3)));  // Legal pawn push
}

TEST(is_legal, castling_check) {
  Board board;
  board.set("r3k2r/8/8/3Q4/5q2/8/8/R3K2R w KQkq - 0 1");
  ASSERT_FALSE(board.is_legal<White>(MoveUtils::encode<KingCastle>(E1, H1)));   // Castling through check
  ASSERT_FALSE(board.is_legal<White>(MoveUtils::encode<QueenCastle>(E1, A1)));  // Castling through check
  ASSERT_TRUE(board.is_legal<White>(MoveUtils::encode<Quiet>(E1, D1)));         // Legal king move
}

}  // namespace Lyra
