#pragma once

#include <atomic>

#include "board.hpp"
#include "clock.hpp"
#include "defs.hpp"

namespace Lyra {

class ThreadPool;
class Thread;

struct PVLine {
  Move   moves[MAX_DEPTH];
  size_t length;

  void        update(const PVLine& other, Move best);
  void        clear() { length = 0; }
  std::string to_str();
};

struct StackEntry {
  Move  killers[2];
  Move  curr_move;
  Move  excl_move;
  Piece moved;
  Eval  eval;
  bool  in_check;
  Ply   ply_since_null;
};

class Worker {
  enum NodeType { Root, PV, NonPV };

 public:
  Worker(std::atomic_bool& stop, size_t id) : clock_(stop), stop_(stop), id_(id) {}
  bool is_main() { return id_ == 0; }

  void reset(std::string fen);
  void start(const TimeControl& tc);

  void report();

 private:
  void iter_deep();
  void asp_win();
  template <Colour Us, NodeType NT>
  Eval search(Board& board, PVLine& pv, Eval alpha, Eval beta, Depth depth);
  Eval search(Board& board, PVLine& pv, Eval alpha, Eval beta, Depth depth);
  template <Colour Us, NodeType NT>
  Eval qsearch(Board& board, PVLine& pv, Eval alpha, Eval beta);

  Clock             clock_;
  std::atomic_bool& stop_;

  Board  board_;
  size_t id_;

  PVLine pv_;
  size_t nodes_;
  Depth  depth_;
  Ply    ply_;
  Eval   eval_;
};

}  // namespace Lyra
