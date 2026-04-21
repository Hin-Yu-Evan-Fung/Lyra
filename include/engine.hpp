#pragma once

#include "board.hpp"
#include "defs.hpp"
#include "perft.hpp"
#include "search.hpp"
#include "thread.hpp"

#include <string>
#include <vector>

namespace Lyra {

/******************************************\
|==========================================|
|              Engine Config               |
|==========================================|
\******************************************/

class Engine {
public:
  Engine();

  void wait() { pool_.wait(); }
  bool is_busy() { return pool_.is_busy(); }
  void set_pos(const std::string fen, const std::vector<std::string> &moves);
  void print_pos() { board_.print(); }

  void perft(PerftMode pm, Depth d);
  void perft_bench();
  void go(const TimeControl &tc);
  void newgame();
  void stop() { pool_.stop(); }

  void set_threads(size_t num);
  void set_chess960(bool chess960);

private:
  Board      board_;
  ThreadPool pool_;
};

} // namespace Lyra
