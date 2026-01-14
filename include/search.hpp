#pragma once

#include <atomic>

#include "board.hpp"
#include "clock.hpp"
#include "defs.hpp"

namespace Lyra {

class ThreadPool;
class Thread;

struct PVLine {
  Move   moves[MaxDepth];
  size_t length;

  void        update(const PVLine& other, Move best);
  void        clear() { length = 0; }
  std::string format(bool chess960);
};

struct StackEntry {
  Move  killers[2];
  Move  curr_move;
  Move  excl_move;
  Piece moved;
  Eval  eval;
  bool  in_check;
  U16   ply_since_null;
};

class Worker {
  enum NodeType { Root, PV, NonPV };

  template <Colour Us>
  void aspwin();
  template <Colour Us, NodeType NT = Root>
  Eval search(Board& board, PVLine& pv, Eval alpha, Eval beta, Depth depth);
  template <Colour Us, NodeType NT = Root>
  Eval qsearch(Board& board, PVLine& pv, Eval alpha, Eval beta);

  Clock             clock_;
  std::atomic_bool& stop_;

  Board  root_board_;
  size_t id_;

  PVLine pv_;
  size_t nodes_;
  Depth  depth_;
  Depth  seldepth_;
  U16    ply_;
  Eval   eval_;

 public:
  Worker(std::atomic_bool& stop, size_t id) : clock_(stop), stop_(stop), id_(id) {}
  bool is_main() { return id_ == 0; }

  void reset(const Board& board);
  void start(const TimeControl& tc);
  void uci_report();
};

}  // namespace Lyra
