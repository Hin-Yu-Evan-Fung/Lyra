#pragma once

#include <string>
#include <vector>

#include "board.hpp"
#include "defs.hpp"
#include "perft.hpp"
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
constexpr size_t THREADS           = 1;
constexpr int    MOVE_OVERHEAD     = 300;
constexpr U64    CLOCK_FREQ        = 2048;

class Engine {
 public:
  Engine();

  void wait() { pool_.wait(); }
  bool is_busy() { return pool_.is_busy(); }
  void set_pos(const std::string fen, const std::vector<std::string>& moves);
  void print_pos() { board_.print(); }

  template <PerftMode PM>
  void perft(Depth d);
  void perft_bench();
  void go(const TimeControl& tc);
  void newgame();
  void stop() { pool_.stop(); }

  void set_threads(size_t num);
  void set_chess960(bool chess960);

 private:
  Board      board_;
  ThreadPool pool_;
};

}  // namespace Lyra
