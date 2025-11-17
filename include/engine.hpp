#pragma once

#include <string>
#include <vector>

#include "board.hpp"
#include "defs.hpp"
#include "search.hpp"
#include "thread.hpp"

namespace Lyra {

/******************************************\
|==========================================|
|              Engine Config               |
|==========================================|
\******************************************/

constexpr std::string_view NAME    = "Lyra";
constexpr std::string_view AUTHOR  = "Evan Fung";
constexpr std::string_view VERSION = "1.0";

constexpr size_t HASH_SIZE         = 32;
constexpr size_t THREADS           = 64;
constexpr int    MOVE_OVERHEAD     = 300;

class Engine {
public:
    Engine();

    void wait_for_search_finish();
    void set_pos(const std::string fen, const std::vector<std::string>& moves);
    void print_pos();

    void perft(Depth d);
    void go(SearchConfig sc);
    void stop();
    void newgame();

private:
    Board      board_;
    ThreadPool pool_;
};

}  // namespace Lyra
