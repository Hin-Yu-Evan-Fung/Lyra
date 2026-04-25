#pragma once

#include "board.hpp"
#include "clock.hpp"
#include "defs.hpp"
#include "move.hpp"
#include "movepick.hpp"
#include "search_utils.hpp"

#include <atomic>

namespace Lyra {

class ThreadPool;
class Thread;

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

  // Move wrappers
  template <Colour Us>
  constexpr void do_move(StackEntry *se, Move move);
  template <Colour Us>
  constexpr void undo_move(StackEntry *se);
  template <Colour Us>
  constexpr void do_null_move(StackEntry *se);
  template <Colour Us>
  constexpr void undo_null_move(StackEntry *se);

  void    update_all_stats(StackEntry *se, Depth depth, Move best);
  MOStats mostats(StackEntry *se);

  constexpr bool can_lmr(Depth depth, int move_count, bool pv, Move move) const;

  Clock             clock_;
  std::atomic_bool &stop_;

  Board  board_;
  size_t id_;

  size_t nodes_;
  Move   best_move_;
  Depth  depth_;
  Depth  last_best_move_depth_;
  Depth  seldepth_;
  Ply    ply_;
  Ply    ply_from_null_;
  Eval   eval_;
  Eval   avg_eval_;

  HistQuiet hist_quiet_;

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

/******************************************\
|==========================================|
|            Do Move / Undo Move           |
|==========================================|
\******************************************/

template <Colour Us>
constexpr void Worker::do_move(StackEntry *se, Move move) {
  ++nodes_;
  ++ply_;

  se->ply_from_null = ply_from_null_++;
  se->move          = move;
  board_.do_move<Us>(move);
}

template <Colour Us>
constexpr void Worker::undo_move(StackEntry *se) {
  --ply_;
  ply_from_null_ = se->ply_from_null;
  board_.undo_move<Us>();
}

template <Colour Us>
constexpr void Worker::do_null_move(StackEntry *se) {
  ++nodes_;
  ++ply_;

  se->ply_from_null = ply_from_null_;
  ply_from_null_    = 0;
  se->move          = NullMove;
  board_.do_null_move<Us>();
}

template <Colour Us>
constexpr void Worker::undo_null_move(StackEntry *se) {
  --ply_;
  ply_from_null_ = se->ply_from_null;
  board_.undo_null_move<Us>();
}

/******************************************\
|==========================================|
|            Pruning Conditions            |
|==========================================|
\******************************************/

constexpr bool Worker::can_lmr(Depth depth, int move_count, bool pv, Move move) const {
  return depth > 2 && move_count > 2 + pv && !MoveUtils::is_capture(move)
         && !MoveUtils::is_promo(move);
}

} // namespace Lyra
