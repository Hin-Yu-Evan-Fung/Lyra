#pragma once

#include "defs.hpp"
#include "history.hpp"
#include "params.hpp"
#include "tt.hpp"

#include <functional>
namespace Lyra {

struct PVLine {
  Move   moves[MaxDepth];
  size_t length;

  void        update(const PVLine &other, Move best);
  void        clear() { length = 0; }
  std::string format(bool chess960) const;
};

struct StackEntry {
  PVLine     pv;
  Killer     killer;
  HistQuiet *cont;
  Eval       eval;
  Ply        ply_from_null;
  Move       move;
  Move       excl;
};

/******************************************\
|==========================================|
|            Search IO functions           |
|==========================================|
\******************************************/

struct PrintInfo {
  Depth       depth;
  Depth       seldepth;
  Eval        eval;
  Time        time;
  U64         nodes;
  U64         nps;
  U16         hashfull;
  std::string pv;
};

struct WorkerCallbacks {
  std::function<void(Move)>      on_best_move;
  std::function<void(PrintInfo)> on_depth_finished;
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
