#include "bitboard.hpp"
#include "perft.hpp"
#include "zobrist.hpp"

using namespace Lyra;

int main() {
    BBUtils::init();
    Zobrist::init();

    for (int i = 0; i < 10; i++)
        perft_bench();

    return 0;
}
