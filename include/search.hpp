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
  std::string format(bool chess960) const;
};

struct StackEntry {
  std::array<Move, 2> killers;
  PVLine              pv;
  U16                 ply;
};

class Worker {
  enum NodeType { Root, PV, NonPV };

  template <Colour Us>
  void aspwin();
  template <Colour Us, NodeType NT = Root>
  Eval search(Board& board, StackEntry* ss, Eval alpha, Eval beta, Depth depth);
  template <Colour Us, NodeType NT = Root>
  Eval qsearch(Board& board, StackEntry* ss, Eval alpha, Eval beta);

  Clock             clock_;
  std::atomic_bool& stop_;

  Board  root_;
  size_t id_;

  size_t nodes_;
  Depth  depth_;
  Depth  seldepth_;
  Eval   eval_;
  Move   best_move_;

 public:
  Worker(std::atomic_bool& stop, size_t id) : clock_(stop), stop_(stop), id_(id) {}
  bool is_main() { return id_ == 0; }

  void reset(const Board& board);
  void start(const TimeControl& tc);
  void uci_report(const PVLine& pv);
};

}  // namespace Lyra
