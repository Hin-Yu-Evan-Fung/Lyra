#pragma once

#include "board.hpp"
#include "clock.hpp"
#include "defs.hpp"
#include "history.hpp"
#include "movepick.hpp"
#include "search_utils.hpp"
#include "tt.hpp"

#include <atomic>

namespace Lyra {

class ThreadPool;
class Thread;

class Worker {
  enum NodeType { PV, NonPV };

  template <Colour Us>
  void aspwin();
  template <Colour Us, NodeType NT>
  Eval search(StackEntry *se, Eval alpha, Eval beta, Depth depth);
  template <Colour Us, NodeType NT>
  Eval    qsearch(StackEntry *se, Eval alpha, Eval beta);
  MOStats mostats(StackEntry *se);

  Clock             clock_;
  std::atomic_bool &stop_;

  Board  board_;
  size_t id_;

  size_t nodes_;
  Depth  depth_;
  Depth  seldepth_;
  Eval   eval_;
  Move   best_move_;

  MainHist history_;

  TT &tt_;

public:
  Worker(std::atomic_bool &stop, size_t id, TT &tt)
      : clock_(stop)
      , stop_(stop)
      , id_(id)
      , tt_(tt) {}
  bool         is_main() { return id_ == 0; }
  const Clock &clock() { return clock_; }
  size_t       nodes() { return nodes_; }

  void reset(const Board &board);
  void start(TimeControl tc);
  void uci_report(const PVLine &pv) const;
  void report_best_move() const;
};

} // namespace Lyra
