#include "board/board.hpp"
#include "board/movegen.hpp"

#include <gtest/gtest.h>

namespace Lyra {

struct SeeTestCase {
  std::string fen;
  std::string move;
  Eval        threshold;
  bool        expected;
};

constexpr Eval PIECE_VALS[NPieceType] = {150, 340, 360, 480, 1000, 0};

const SeeTestCase cases[] = {
    {"2k5/8/8/4p3/8/8/2K1R3/8 w - - 0 1", "e2e5", 0, true},
    {"3k4/8/8/4p3/3P4/8/8/5K2 w - - 0 1", "d4e5", PIECE_VALS[P], true},
    {"3k4/8/2p1p3/3p4/2P1P3/8/8/5K2 w - - 0 1", "c4d5", PIECE_VALS[P], false},
    {"8/3k4/2n2b2/8/3P4/8/3KN3/8 b - - 0 1", "c6d4", PIECE_VALS[P], true},
    {"8/3k4/2n2b2/8/3P4/8/3KN3/8 b - - 0 1", "c6d4", PIECE_VALS[N], false},
    {"3kr3/8/4q3/8/4P3/5P2/8/3K4 b - - 0 1", "e6e4", 0, false},
    {"3kr3/8/4q3/8/4P3/5P2/8/3K4 b - - 0 1", "e6e4", -PIECE_VALS[Q], true},
    {"8/3k4/2n2b2/8/3P4/3K4/4N3/8 b - - 0 1", "c6d4", PIECE_VALS[P], false},
    {"5k2/2P5/4b3/8/8/8/8/2R2K2 w - - 0 1", "c7c8q", 0, true},
    {"5k2/2P5/4b3/8/8/8/8/3R1K2 w - - 0 1", "c7c8q", 0, false},
    {"8/3k2b1/2n2b2/8/3P4/3K4/4N3/8 b - - 0 1", "c6d4", 0, true},
    {"3k4/8/2q5/2b5/2r5/8/2P5/2R1K3 b - - 0 1", "c4c2", 0, false},
    {"3k4/8/2q5/2b5/2r5/8/2P5/2R1K3 b - - 0 1", "c4c2", PIECE_VALS[P] - PIECE_VALS[R], true},
    {"2k5/3n2b1/2nq4/4R3/5P2/3N1N2/8/5K2 b - - 0 1", "d6e5", 0, false},
    {"2k5/3n2b1/2nq4/4R3/5P2/3N1N2/8/5K2 b - - 0 1", "d6e5",
     PIECE_VALS[R] - PIECE_VALS[Q] + PIECE_VALS[P], true},
    {"5r1k/3b1q1p/1npb4/1p6/pPpP1N2/2P4B/2NBQ1P1/5R1K b - - 0 1", "d6f4", 0, false},
    {"5r1k/3b1q1p/1npb4/1p6/pPpP1N2/2P4B/2NBQ1P1/5R1K b - - 0 1", "d6f4", -PIECE_VALS[P], true},
}; // namespace Lyra

TEST(see, see_bench) {
  Board board;

  for (auto &[fen, move_str, threshold, expected] : cases) {
    board.set(fen);

    Move move = NoMove;
    for (Move m : list_moves(board)) {
      if (move_str == MoveUtils::format(m, board.chess960)) {
        move = m;
        break;
      }
    }

    ASSERT_NE(move, NoMove);
    ASSERT_EQ(board.see(move, threshold), expected);
  }
}

// TEST(see, simple_winning_capture) {
//   Board board;
//   board.set("r1k5/8/8/8/8/8/8/Q6K w - - 0 1");
//   ASSERT_TRUE(board.see<White>(MoveUtils::encode<Cap>(A1, A8), EvalDraw));
// }
//
// TEST(see, simple_losing_capture) {
//   Board board;
//   board.set("2q5/1k1p4/8/8/Q7/8/8/7K w - - 0 1");
//   ASSERT_FALSE(board.see<White>(MoveUtils::encode<Cap>(A4, D7), EvalDraw));
// }
//
// TEST(see, equal_exchange) {
//   Board board;
//   board.set("rk6/8/8/8/8/8/8/RK6 w - - 0 1");
//   ASSERT_TRUE(board.see<White>(MoveUtils::encode<Cap>(A1, A8), EvalDraw));
// }

} // namespace Lyra
