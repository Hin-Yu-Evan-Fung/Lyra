#pragma once

#include "board/board.hpp"
#include "core/defs.hpp"
#include "engine/clock.hpp"
#include "search/utils.hpp"
#include "utils/tt.hpp"

#include <atomic>

namespace Lyra {

using namespace SearchUtils;

class ThreadPool;
class Thread;

class Worker {
  enum NodeType { PV, NonPV };

  template <Colour Us> void aspwin();
  template <Colour Us, NodeType NT> Eval negamax(StackEntry *se, Eval alpha, Eval beta, Depth depth);
  template <Colour Us> Eval nw_search(StackEntry *se, Eval upper, Depth depth);
  template <Colour Us, NodeType NT> Eval qsearch(StackEntry *se, Eval alpha, Eval beta);

  template <Colour Us> void do_move(StackEntry *se, Move move);
  template <Colour Us> void undo_move(StackEntry *se);
  template <Colour Us> void do_null_move(StackEntry *se);
  template <Colour Us> void undo_null_move(StackEntry *se);

  template <Colour Us> bool can_nmp(StackEntry *se, Depth depth, Eval eval, Eval beta);

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

public:
  Worker(std::atomic_bool &stop, size_t id, TT &tt)
      : clock_(stop)
      , stop_(stop)
      , id_(id)
      , tt_(tt) {}
  bool is_main() { return id_ == 0; }

  void reset(const Board &board);
  void start(TimeControl tc);
  void uci_report(const PVLine &pv);
};

/******************************************\
|==========================================|
|         Templated Search Helpers         |
|==========================================|
\******************************************/

template <Colour Us> void Worker::do_move(StackEntry *se, Move move) {
  board_.do_move<Us>(move);
  nodes_++;
  se->ply_from_null++;
}

template <Colour Us> void Worker::undo_move(StackEntry *se) {
  board_.undo_move<Us>();
  se->ply_from_null = (se - 1)->ply_from_null;
}

template <Colour Us> void Worker::do_null_move(StackEntry *se) {
  board_.do_null_move<Us>();
  se->ply_from_null = 0;
}

template <Colour Us> void Worker::undo_null_move(StackEntry *se) {
  board_.undo_null_move<Us>();
  se->ply_from_null = (se - 1)->ply_from_null;
}

/******************************************\
|==========================================|
|            Pruning Conditions            |
|==========================================|
\******************************************/

template <Colour Us> bool Worker::can_nmp(StackEntry *se, Depth depth, Eval eval, Eval beta) {
  return depth >= 2 && se->ply_from_null > 0 && eval >= beta && beta >= -EvalMateBound
         && board_.has_non_pawn_material<Us>();
}

} // namespace Lyra
