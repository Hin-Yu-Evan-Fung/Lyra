#pragma once

#include "defs.hpp"
#include "history.hpp"
#include "params.hpp"
#include "tt.hpp"
namespace Lyra {

struct PVLine {
  Move   moves[MaxDepth];
  size_t length;

  void        update(const PVLine &other, Move best);
  void        clear() { length = 0; }
  std::string format(bool chess960) const;
};

struct StackEntry {
  PVLine pv;
  Killer killer;
  Eval   eval;
  Ply    ply_from_null;
  Move   move;
};

/******************************************\
|==========================================|
|               Search Utils               |
|==========================================|
\******************************************/

// We can use eval if it is tighter than bounded
constexpr bool can_use_bound(Bound bound, Eval eval, Eval bounded) {
  return bound & (eval >= bounded ? Bound::Lower : Bound::Upper);
}

} // namespace Lyra
