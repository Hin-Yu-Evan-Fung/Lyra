#pragma once

#include "board.hpp"
#include "clock.hpp"
#include "defs.hpp"
#include "history.hpp"
#include "move.hpp"
#include "movepick.hpp"
#include "params.hpp"
#include "search_utils.hpp"
#include "tt.hpp"

#include <atomic>
#include <cmath>

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
  Eval negamax(StackEntry *se, Eval alpha, Eval beta, Depth depth, bool cutnode);
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

  void    update_all_stats(StackEntry *se, Depth depth, Move best, std::vector<Move> &captures,
                           std::vector<Move> &quiets);
  void    update_hist_cont(StackEntry *se, Move move, Eval bonus);
  MOStats mostats(StackEntry *se);

  Eval adjust_eval(Eval eval) const;
  bool is_improving(StackEntry *se) const;

  constexpr bool can_lmr(Depth depth, int move_count, bool pv, Move move) const;
  constexpr bool can_nmp(StackEntry *se, Depth depth, Eval eval, Eval beta) const;
  constexpr bool can_fp(Depth lmr_depth, Eval eval, Eval alpha) const;
  constexpr bool can_rfp(Depth depth, Eval eval, Eval beta) const;
  constexpr bool can_lmp(Depth depth, int move_count, bool improving) const;
  constexpr bool can_see(Depth depth, Move move, Eval best) const;
  constexpr bool can_singular(Bound tt_bound, Depth tt_depth, Move tt_move, Eval tt_value,
                              Depth depth, Move move) const;

  // Reductions
  constexpr Depth lmr_reduction(Depth depth, int move_count);
  constexpr Depth nmp_reduction(Depth depth) const;

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

  TT &tt_;

  HistQuiet hist_quiet_;
  HistCont  hist_cont_;
  HistCorr  hist_corr_;

public:
  // Stats terms
  U64 cutoffs_;
  U64 first_move_cutoffs_;
  U64 lmr_searches_;
  U64 lmr_researches_;

  Worker(std::atomic_bool &stop, size_t id, TT &tt)
      : clock_(stop)
      , stop_(stop)
      , id_(id)
      , tt_(tt) {}
  bool is_main() { return id_ == 0; }

  void reset(const Board &board);
  void start(TimeControl tc);

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
  const PieceTo p = piece_to(board_, move);
  ++nodes_;
  ++ply_;

  se->ply_from_null = ply_from_null_++;
  se->move          = move;
  se->cont          = &hist_cont_[p.pc][p.to];
  board_.do_move<Us>(move);
  tt_.prefetch(board_.key());
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
  se->cont          = &hist_cont_[wP][A1]; // Dummy table
  board_.do_null_move<Us>();
  tt_.prefetch(board_.key());
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

constexpr bool Worker::can_nmp(StackEntry *se, Depth depth, Eval eval, Eval beta) const {
  return depth >= 2 && (se - 1)->move != NullMove && eval >= beta && !is_win(eval) && !is_loss(beta)
         && board_.has_non_pawn_material(board_.stm());
}

constexpr bool Worker::can_fp(Depth lmr_depth, Eval eval, Eval alpha) const {
  return lmr_depth <= 5 && eval + 100 * lmr_depth + 100 < alpha;
}

constexpr bool Worker::can_rfp(Depth depth, Eval eval, Eval beta) const {
  return depth <= 8 && !is_win(eval) && !is_loss(beta) && eval - 100 * depth > beta;
}

constexpr bool Worker::can_lmp(Depth depth, int move_count, bool improving) const {
  return depth <= 8 && move_count >= (3 + depth * depth) / (2 - improving);
}

constexpr bool Worker::can_see(Depth depth, Move move, Eval best) const {
  return depth <= 10 && !is_loss(best)
         && !board_.see(move, MoveUtils::is_capture(move) ? -20 * depth * depth : -70 * depth);
}

constexpr bool Worker::can_singular(Bound tt_bound, Depth tt_depth, Move tt_move, Eval tt_value,
                                    Depth depth, Move move) const {
  return move == tt_move && depth >= 8 && !is_terminal(tt_value) && (tt_bound & Bound::Lower)
         && tt_depth >= depth - 3;
}

/******************************************\
|==========================================|
|                Reductions                |
|==========================================|
\******************************************/

constexpr Depth Worker::lmr_reduction(Depth depth, int move_count) {
  const float lmr_base = LmrBaseQuiet / 1024.0;
  const float lmr_mult = LmrMultQuiet / 1024.0;
  return lmr_base + std::log(depth) * std::log(move_count) / lmr_mult;
}

constexpr Depth Worker::nmp_reduction(Depth depth) const { return 3 + depth / 5; }

} // namespace Lyra
