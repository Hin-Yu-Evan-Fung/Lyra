#pragma once

#include <atomic>

#include "board.hpp"
#include "clock.hpp"
#include "defs.hpp"
#include "history.hpp"
#include "tt.hpp"

namespace Lyra {

class ThreadPool;
class Thread;

struct PVLine {
  Move moves[MaxDepth];
  size_t length;

  void update(const PVLine &other, Move best);
  void clear() { length = 0; }
  std::string format(bool chess960) const;
};

struct StackEntry {
  Killer killer;
  PVLine pv;
  U16 ply;
};

class Worker {
  enum NodeType { PV, NonPV };

  template <Colour Us> void aspwin();
  template <Colour Us, NodeType NT>
  Eval search(StackEntry *ss, Eval alpha, Eval beta, Depth depth);
  template <Colour Us, NodeType NT>
  Eval qsearch(StackEntry *ss, Eval alpha, Eval beta);

  // Worker info
  Clock clock_;
  std::atomic_bool &stop_;
  Board board_;
  size_t id_;

  // Search info
  size_t nodes_;
  Depth depth_;
  Depth seldepth_;
  Eval eval_;
  Move best_move_;

  // Tables
  MainHistory history_;
  TT &tt_;

  // Stats
  U64 tt_reads;
  U64 tt_hits;

public:
  Worker(std::atomic_bool &stop, size_t id, TT &tt)
      : clock_(stop), stop_(stop), id_(id), tt_(tt) {}
  bool is_main() { return id_ == 0; }

  void reset(const Board &board);
  void start(TimeControl tc);
  void uci_report(const PVLine &pv);
};

} // namespace Lyra
