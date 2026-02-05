#pragma once

#include "core/defs.hpp"
#include "search/history.hpp"
#include "utils/tt.hpp"
#include <cmath>

namespace Lyra::SearchUtils {

using MoveBuf = std::vector<Move>;

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

struct MOStats {
  MainHistory ht;

  void clear();
  void update(const Board &board, StackEntry *ss, MoveBuf captures,
              MoveBuf quiets, Move best_move, Depth depth);
};

/******************************************\
|==========================================|
|               Search Utils               |
|==========================================|
\******************************************/

// Variable reduction formula for lmr
constexpr Depth lmr_reduction(Depth depth, int move_count) {
  return 1 + std::log(depth) * std::log(move_count) / 3.5;
}

// Exact bound means value has been proven with full window search. Can cutoff.
// Upper bound means value has a proven upper bound, can cutoff if alpha is too
// good to be true.
// Lower bound means value has a proven lower bound, can cutoff if beta is too
// good to be true;
constexpr bool can_tt_cutoff(const TTEntry &entry, Eval alpha, Eval beta) {
  const Eval value = entry.value;
  return std::array{false, true, value <= alpha, value >= beta}[entry.bound];
}

} // namespace Lyra::SearchUtils
