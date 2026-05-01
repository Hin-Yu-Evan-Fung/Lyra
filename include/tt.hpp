#pragma once

#include "defs.hpp"
#include "params.hpp"

namespace Lyra {

/******************************************\
|==========================================|
|      Transposition Table Definitions     |
|==========================================|
\******************************************/

// | Field | Bits | Offset
// | ----- | ---- | ------
// | age   | 6    | 0
// | bound | 2    | 6
// | depth | 8    | 8
// | move  | 16   | 16
// | eval  | 16   | 32
// | value | 16   | 48

enum Bound {
  None,  // Writing static eval to TT
  Lower, // Fail high nodes, lower bound for position eval
  Upper, // Fail low nodes, upper bound for position eval
  Exact, // PV nodes
};

struct TTData {
  Bound bound;
  Depth depth;
  Move  move;
  Eval  eval;
  Eval  value;

  TTData() = delete;

  TTData(Bound b, Depth d, Move m, Eval ev, Eval v)
      : bound(b)
      , depth(d)
      , move(m)
      , eval(ev)
      , value(v) {}
};

struct TTEntry {
  U16  key16;
  Age  age_bound8;
  U8   depth8;
  Move move16;
  I16  eval16;
  I16  value16;

  Age    relative_age(Age curr) const { return (curr - age_bound8) & AgeMask; }
  bool   is_occupied() const { return bool(depth8); }
  TTData read(Ply p) const;
  void   save(Key k, Age curr, Bound b, Depth d, Ply p, Move m, Eval ev, Eval v);
};

using TTResult = std::tuple<bool, TTEntry *, TTData>;

class TT {
  TTEntry *entries_;
  size_t   n_entries_;
  Age      age8_;

  constexpr size_t calc_no_of_entries(size_t mb) const { return (mb << 20) / sizeof(TTEntry); }
  constexpr size_t index(Key key) const { return (U128(key) * U128(n_entries())) >> 64; }

public:
  U64 tt_hits_;
  U64 tt_collisions_;

  TT(size_t mb)
      : entries_{nullptr}
      , age8_{0} {
    resize(mb);
    clear();
  }

  constexpr Age    age() const { return age8_; }
  constexpr size_t n_entries() const { return n_entries_; }
  size_t           hashfull() const;

  void reset_age() { age8_ = 0; }
  void incr_age() { age8_ = (age8_ + 1) & AgeMask; }
  void resize(size_t mb);
  void clear();

  void     prefetch(Key key) const;
  TTResult probe(Key key, Ply ply);
};

} // namespace Lyra
