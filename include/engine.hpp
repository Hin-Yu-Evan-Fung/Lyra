#pragma once

#include "board.hpp"
#include "defs.hpp"
#include "perft.hpp"
#include "search.hpp"
#include "thread.hpp"
#include "tt.hpp"

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
  void print_eval();

  void perft(PerftMode perft_mode, Depth d);
  void go(const TimeControl &tc);
  void newgame();
  void stop() { pool_.stop(); }

  void clear_tt();
  void set_threads(size_t num);
  void set_tt_size(size_t mb);
  void set_chess960(bool chess960);

private:
  Board      board_;
  ThreadPool pool_;
  TT         tt_;
};

} // namespace Lyra
