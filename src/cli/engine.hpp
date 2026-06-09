#pragma once

#include "board.hpp"
#include "defs.hpp"
#include "perft.hpp"
#include "search.hpp"
#include "search_utils.hpp"
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
  void set_pos(const std::string fen, const std::vector<std::string> &moves);
  void print_pos() { board_.print(); }

  void perft(PerftMode pm, Depth d);
  void perft_bench();
  void go(const TimeControl &tc);
  void newgame();
  void stop() { pool_.stop(); }

  void clear_tt();
  void set_threads(size_t num);
  void set_tt_size(size_t mb);
  void set_chess960(bool chess960);

  void on_best_move(Move m) const;
  void on_depth_finished(PrintInfo info) const;

private:
  Board           board_;
  TT              tt_;
  ThreadPool      pool_;
  WorkerCallbacks callbacks_;
};

} // namespace Lyra
