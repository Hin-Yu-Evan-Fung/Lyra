#pragma once

#include "board/board.hpp"
#include "core/defs.hpp"
#include "core/move.hpp"
#include "engine/clock.hpp"
#include "engine/params.hpp"
#include "search/movepick.hpp"
#include "search/utils.hpp"
#include "utils/bench.hpp"
#include "utils/tt.hpp"
#include "utils/utils.hpp"

#include <atomic>

namespace Lyra {

using namespace SearchUtils;
using namespace EvalUtils;

class ThreadPool;
class Thread;

class Worker {
  enum NodeType { PV, NonPV };

  template <Colour Us> void aspwin();
  template <Colour Us, NodeType NT>
  Eval negamax(StackEntry *se, Eval alpha, Eval beta, Depth depth, bool cutnode);
  template <Colour Us> Eval nw_search(StackEntry *se, Eval upper, Depth depth, bool cutnode);
  template <Colour Us, NodeType NT> Eval qsearch(StackEntry *se, Eval alpha, Eval beta);

  template <Colour Us> void do_move(StackEntry *se, Move move);
  template <Colour Us> void undo_move(StackEntry *se);
  template <Colour Us> void do_null_move(StackEntry *se);
  template <Colour Us> void undo_null_move(StackEntry *se);

  constexpr bool can_rfp(Depth depth, Eval eval, Eval beta);
  constexpr bool can_razor(Depth depth, Eval eval, Eval alpha);
  constexpr bool can_lmp(Depth depth, U16 move_count);
  constexpr bool can_see_prune(Depth depth, Move move, Eval best);
  constexpr bool can_lmr(Depth depth, U16 move_count, bool pv, Move move);
  constexpr bool can_nmp(StackEntry *se, Depth depth, Eval eval, Eval beta);
  constexpr bool can_singular(Depth depth, Depth tt_depth, TTBound tt_bound, Eval tt_value);

  MOStats mo_stats(StackEntry *se);
  void    update_cont(const Board &board, StackEntry *se, Move move, Eval bonus);
  void    update_hist(const Board &board, StackEntry *se, MoveBuf captures, MoveBuf quiets,
                      Move best_move, Depth depth);

  // Worker info
  Clock             clock_;
  std::atomic_bool &stop_;
  Board             board_;
  size_t            id_;

  // Search info
  size_t nodes_;
  Depth  depth_;
  Depth  seldepth_;
  Eval   eval_;
  Move   best_move_;

  // Tables
  MainHist  ht_;
  CapHist   cht_;
  ContTable cont_tb_;
  TT       &tt_;

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

  friend void run_bench(int argc, char **argv);
};

/******************************************\
|==========================================|
|         Templated Search Helpers         |
|==========================================|
\******************************************/

template <Colour Us> void Worker::do_move(StackEntry *se, Move move) {
  se->cont = &cont_tb_.probe(board_, move);
  se->ply_from_null++;
  se->move = move;
  nodes_++;
  board_.do_move<Us>(move);
}

template <Colour Us> void Worker::undo_move(StackEntry *se) {
  se->ply_from_null = (se - 1)->ply_from_null;
  board_.undo_move<Us>();
}

template <Colour Us> void Worker::do_null_move(StackEntry *se) {
  se->cont          = &cont_tb_[wP][A1];
  se->ply_from_null = 0;
  se->move          = NullMove;
  nodes_++;
  board_.do_null_move<Us>();
}

template <Colour Us> void Worker::undo_null_move(StackEntry *se) {
  se->ply_from_null = (se - 1)->ply_from_null;
  board_.undo_null_move<Us>();
}

/******************************************\
|==========================================|
|            Pruning Conditions            |
|==========================================|
\******************************************/

constexpr bool Worker::can_rfp(Depth depth, Eval eval, Eval beta) {
  return !is_win(eval) && !is_loss(beta) && depth <= RFPDepth && eval - RFPFactor * depth >= beta;
}

constexpr bool Worker::can_razor(Depth depth, Eval eval, Eval alpha) {
  return !is_win(alpha) && eval < alpha - RazorConstFactor - RazorQuadFactor * depth * depth;
}

constexpr bool Worker::can_lmp(Depth depth, U16 move_count) {
  return depth <= 8 && move_count >= 3 + depth * depth;
}

constexpr bool Worker::can_see_prune(Depth depth, Move move, Eval best) {
  return !is_terminal(best) && depth <= 10
         && !board_.see(move, is_capture(move) ? -20 * depth * depth : -70 * depth);
}

constexpr bool Worker::can_lmr(Depth depth, U16 move_count, bool pv, Move move) {
  return depth >= 2 && move_count > 2 + pv && !is_promo(move) && !is_capture(move);
}

constexpr bool Worker::can_nmp(StackEntry *se, Depth depth, Eval eval, Eval beta) {
  return depth >= 2 && se->ply_from_null > 0 && eval >= beta && !is_loss(beta)
         && board_.has_non_pawn_material(board_.stm());
}

constexpr bool Worker::can_singular(Depth depth, Depth tt_depth, TTBound tt_bound, Eval tt_value) {
  return depth >= 8 && tt_depth >= depth - 3 && (tt_bound & TTBound::Lower)
         && !is_terminal(tt_value);
}

} // namespace Lyra
