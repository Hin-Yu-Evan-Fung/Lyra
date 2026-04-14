#pragma once

#include "board.hpp"
#include "clock.hpp"
#include "defs.hpp"
#include "history.hpp"
#include "move.hpp"
#include "movepick.hpp"
#include "search_utils.hpp"
#include "tt.hpp"

#include <atomic>
#include <cmath>

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
  void    update_cont_table(StackEntry *se, Move move, Eval bonus);
  void update_all_stats(StackEntry *se, Depth depth, Move best, const std::vector<Move> &captures,
                        const std::vector<Move> &quiets);

  // Move wrappers
  template <Colour Us>
  constexpr void do_move(StackEntry *se, Move move);
  template <Colour Us>
  constexpr void undo_move(StackEntry *se);
  template <Colour Us>
  constexpr void do_null_move(StackEntry *se);
  template <Colour Us>
  constexpr void undo_null_move(StackEntry *se);

  // Pruning conditions
  constexpr bool can_rfp(Depth depth, Eval eval, Eval beta) const;
  constexpr bool can_nmp(StackEntry *se, Depth depth, Eval eval, Eval beta) const;
  constexpr bool can_lmr(Depth depth, int move_count, bool pv) const;
  constexpr bool can_see_prune(Depth depth, Move move, Eval best) const;
  constexpr bool can_lmp(Depth depth, int move_count) const;

  // Reductions
  constexpr Depth lmr_reduction(Depth depth, int move_count, bool is_cap);
  constexpr Depth nmp_reduction(Depth depth);

  Clock             clock_;
  std::atomic_bool &stop_;

  Board  board_;
  size_t id_;

  size_t nodes_;
  Depth  depth_;
  Depth  seldepth_;
  Depth  last_best_move_depth_;
  Eval   eval_;
  Eval   avg_eval_;
  Move   best_move_;

  MainHist  hist_quiet_;
  CapHist   hist_cap_;
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

/******************************************\
|==========================================|
|            Pruning Conditions            |
|==========================================|
\******************************************/

constexpr bool Worker::can_rfp(Depth depth, Eval eval, Eval beta) const {
  return depth <= 8 && eval >= beta && eval - 100 * depth >= beta;
}

constexpr bool Worker::can_nmp(StackEntry *se, Depth depth, Eval eval, Eval beta) const {
  return depth >= 2 && (se - 1)->move != NullMove && eval >= beta && !is_win(eval) && !is_loss(beta)
         && board_.has_non_pawn_material(board_.stm());
}

constexpr bool Worker::can_lmr(Depth depth, int move_count, bool pv) const {
  return depth > 2 && move_count > 2 + pv;
}

constexpr bool Worker::can_see_prune(Depth depth, Move move, Eval best) const {
  using namespace MoveUtils;
  return !is_terminal(best) && depth <= 10
         && !board_.see(move, is_capture(move) ? -70 * depth : -20 * depth * depth);
}

constexpr bool Worker::can_lmp(Depth depth, int move_count) const {
  return move_count >= 3 + depth * depth;
}

/******************************************\
|==========================================|
|                Reductions                |
|==========================================|
\******************************************/

constexpr Depth Worker::lmr_reduction(Depth depth, int move_count, bool is_cap) {
  if (is_cap)
    return 0.35 + std::log(depth) * std::log(move_count) / 3;
  else
    return 0.75 + std::log(depth) * std::log(move_count) / 2.5;
}

constexpr Depth Worker::nmp_reduction(Depth depth) { return 3 + depth / 5; }

} // namespace Lyra
