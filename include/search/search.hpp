#pragma once

#include <atomic>

#include "board/board.hpp"
#include "core/defs.hpp"
#include "engine/clock.hpp"
#include "search/utils.hpp"
#include "utils/tt.hpp"

namespace Lyra {

using namespace SearchUtils;

class ThreadPool;
class Thread;

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
  MOStats stats_;
  TT &tt_;

  // Stats
  U64 tt_reads;
  U64 tt_hits;
  U64 fail_high;
  U64 fail_high_first;
  U64 lmr_searches;
  U64 lmr_researches;

public:
  Worker(std::atomic_bool &stop, size_t id, TT &tt)
      : clock_(stop), stop_(stop), id_(id), tt_(tt) {}
  bool is_main() { return id_ == 0; }

  void reset(const Board &board);
  void start(TimeControl tc);
  void uci_report(const PVLine &pv);
};

} // namespace Lyra
