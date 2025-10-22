
#include <cassert>
#include <iostream>

#include "bitboard.hpp"
#include "board.hpp"
#include "defs.hpp"
#include "move_defs.hpp"

using namespace Lyra;

int main() {
    BBUtils::init();

    Board board;

    board.set("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    board.do_move<White>(MoveUtils::encode<DoublePush>(E2, E4));
    board.undo_move<White>();

    board.print();
}
