#pragma once

#include "defs.hpp"
#include "history.hpp"
#include "tt.hpp"

#include <cstddef>

namespace Lyra {

struct PVLine {
  Move   moves[MaxDepth];
  size_t length;

  void        update(const PVLine &other, Move best);
  void        clear() { length = 0; }
  std::string format(bool chess960) const;
};

struct StackEntry {
  Killer    killer;
  PVLine    pv;
  Ply       ply_from_null;
  Move      move;
  Move      excl;
  ContHist *cont;
  Eval      eval;
};

/******************************************\
|==========================================|
|               Search Utils               |
|==========================================|
\******************************************/

// Whether to use a value given a bound
constexpr bool can_use_val(Bound bound, Eval bounded_val, Eval ref) {
  return bound & (bounded_val >= ref ? Bound::Lower : Bound::Upper);
}

// // Exact bound means value has been proven with full window search. Can cutoff.
// // Upper bound means value has a proven upper bound, can cutoff if alpha is too
// // good to be true.
// // Lower bound means value has a proven lower bound, can cutoff if beta is too
// // good to be true;
// constexpr bool can_tt_cutoff(const TTEntry &entry, Eval alpha, Eval beta) {
//   switch (entry.bound) {
//   case Bound::None: return false;
//   case Bound::Exact: return true;
//   case Bound::Upper: return entry.value <= alpha;
//   case Bound::Lower: return entry.value >= beta;
//   }
//   return false;
// }
//
// constexpr bool can_use_tt_value(const TTEntry &entry, Eval value) {
//   switch (entry.bound) {
//   case Bound::None: return false;
//   case Bound::Exact: return true;
//   case Bound::Upper: return entry.value <= value;
//   case Bound::Lower: return entry.value >= value;
//   }
//   return false;
// }

} // namespace Lyra
