#pragma once

#include <string>
#include <vector>

#include "board.hpp"
#include "defs.hpp"
#include "search.hpp"

namespace Lyra {

class Engine {
public:
    Engine();
    ~Engine();

    void wait_for_search_finish();
    void set_pos(const std::string fen, const std::vector<std::string>& moves);
    void print_pos();

    void perft(Depth d);
    void go(SearchConfig sc);
    void stop();
    void newgame();

private:
    Board board;
};

}  // namespace Lyra
