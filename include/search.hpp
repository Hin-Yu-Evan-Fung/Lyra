#pragma once

#include "board.hpp"
#include "clock.hpp"
#include "defs.hpp"

#include <atomic>

namespace Lyra {

class ThreadPool;
class Thread;

struct PVLine {
  Move   moves[MaxDepth];
  size_t length;

  void        update(const PVLine &other, Move best);
  void        clear() { length = 0; }
  std::string format(bool chess960) const;
};

struct StackEntry {
  PVLine pv;
  Eval   eval;
};

class Worker {
  enum NodeType { PV, NonPV };

  bool should_search_deeper();
  void uci_report(const PVLine &pv);
  void report_best_move();

  template <Colour Us>
  void aspwin(StackEntry *se);

  template <Colour Us, NodeType NT>
  Eval search(StackEntry *se, Eval alpha, Eval beta, Depth depth);
  template <Colour Us, NodeType NT>
  Eval qsearch(StackEntry *se, Eval alpha, Eval beta);

  Clock             clock_;
  std::atomic_bool &stop_;

  Board  board_;
  size_t id_;

  size_t nodes_;
  Move   best_move_;
  Depth  depth_;
  Depth  last_best_move_depth_;
  Depth  seldepth_;
  U16    ply_;
  Eval   eval_;
  Eval   avg_eval_;

public:
  Worker(std::atomic_bool &stop, size_t id)
      : clock_(stop)
      , stop_(stop)
      , id_(id) {}
  bool is_main() { return id_ == 0; }

  void reset(const Board &board);
  void start(const TimeControl &tc);

  const Clock &clock() const { return clock_; }
  const U64    nodes() const { return nodes_; }
};

} // namespace Lyra
