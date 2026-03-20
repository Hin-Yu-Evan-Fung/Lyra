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
  void aspwin(StackEntry *se);
  template <Colour Us, NodeType NT>
  Eval negamax(StackEntry *se, Eval alpha, Eval beta, Depth depth);
  template <Colour Us, NodeType NT>
  Eval qsearch(StackEntry *se, Eval alpha, Eval beta);

  // Helpers
  MOStats mostats(StackEntry *se);
  void    update_cont_hist(StackEntry *se, Move move, Eval bonus);
  void    update_all_stats(StackEntry *se, Depth depth, Move best);

  // Move wrappers
  template <Colour Us>
  constexpr void do_move(StackEntry *se, Move move);
  template <Colour Us>
  constexpr void undo_move(StackEntry *se);
  template <Colour Us>
  constexpr void do_null_move(StackEntry *se);
  template <Colour Us>
  constexpr void undo_null_move(StackEntry *se);

  bool can_lmr(Depth depth, Move move, bool pv, int move_count);
  bool can_nmp(StackEntry *se, Depth depth, Eval eval, Eval beta);
  bool can_see_prune(Depth depth, Eval best, Move move);

  Clock             clock_;
  std::atomic_bool &stop_;

  Board  board_;
  size_t id_;

  size_t nodes_;
  Depth  depth_;
  Depth  seldepth_;
  Eval   eval_;
  Move   best_move_;

  MainHist  history_;
  CapHist   cap_history_;
  ContTable cont_table_;

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

/******************************************\
|==========================================|
|            Do Move / Undo Move           |
|==========================================|
\******************************************/

template <Colour Us>
constexpr void Worker::do_move(StackEntry *se, Move move) {
  ++nodes_;
  PieceTo p = piece_to(board_, move);
  se->cont  = &cont_table_[p.pc][p.to];
  se->move  = move;
  board_.do_move<Us>(move);
}

template <Colour Us>
constexpr void Worker::undo_move(StackEntry *se) {
  board_.undo_move<Us>();
}

template <Colour Us>
constexpr void Worker::do_null_move(StackEntry *se) {
  ++nodes_;
  se->cont = &cont_table_[wP][A1]; // Dummy table
  se->move = NullMove;
  board_.do_null_move<Us>();
}

template <Colour Us>
constexpr void Worker::undo_null_move(StackEntry *se) {
  board_.undo_null_move<Us>();
}

} // namespace Lyra
