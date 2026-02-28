#pragma once

#include "core/defs.hpp"
#include "search/history.hpp"
#include "utils/tt.hpp"

#include <cmath>

namespace Lyra::SearchUtils {

using MoveBuf = std::vector<Move>;

struct PVLine {
  Move   moves[MaxDepth];
  size_t length;

  void        update(const PVLine &other, Move best);
  void        clear() { length = 0; }
  std::string format(bool chess960) const;
};

struct StackEntry {
  Killer    killer;
  ContHist *cont;
  PVLine    pv;
  Move      move;
  Ply       ply;
  Ply       ply_from_null;
  bool      in_check;
};

/******************************************\
|==========================================|
|               Search Utils               |
|==========================================|
\******************************************/

// Variable reduction formula for lmr
constexpr Depth lmr_reduction(Depth depth, int move_count) {
  return 0.75 + std::log(depth) * std::log(move_count) / 3;
}

// Variable reduction formula for nmp
constexpr Depth nmp_reduction(Depth depth) { return std::min(Depth(3 + depth / 5), depth); }

// Exact bound means value has been proven with full window search. Can cutoff.
// Upper bound means value has a proven upper bound, can cutoff if alpha is too
// good to be true.
// Lower bound means value has a proven lower bound, can cutoff if beta is too
// good to be true;
constexpr bool can_tt_cutoff(const TTEntry &entry, Eval alpha, Eval beta) {
  const Eval value = entry.value;
  return std::array{false, true, value <= alpha, value >= beta}[entry.bound];
}

// Exact bound means value has been proven with full window search. Can cutoff.
// Upper bound means value has a proven upper bound, can cutoff if alpha is too
// good to be true.
// Lower bound means value has a proven lower bound, can cutoff if beta is too
// good to be true;
constexpr bool can_use_tt_eval(const TTEntry &entry, Eval static_eval) {
  const Eval value = entry.value;
  return std::array{false, true, value<static_eval, value> static_eval}[entry.bound];
}

} // namespace Lyra::SearchUtils
