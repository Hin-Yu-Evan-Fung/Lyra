#pragma once

#include "core/defs.hpp"

#include <atomic>
#include <utility>

namespace Lyra {

/******************************************\
|==========================================|
|      Transposition Table Definitions     |
|==========================================|
\******************************************/

// | Field | Bits | Offset
// | ----- | ---- | ------
// | age   | 7    | 0
// | depth | 7    | 7
// | bound | 2    | 14
// | move  | 16   | 16
// | eval  | 16   | 32
// | value | 16   | 48

enum TTBound {
  None,  // Writing static eval to TT
  Lower, // Fail high nodes, lower bound for position eval
  Upper, // Fail low nodes, upper bound for position eval
  Exact, // PV nodes
};

struct TTEntry {
  Key     key;
  U8      age;
  Depth   depth;
  TTBound bound;
  Move    move;
  Eval    eval;
  Eval    value;
};

struct PackedTTEntry {
  std::atomic_uint64_t key;
  std::atomic_uint64_t data;

  constexpr bool is_valid(Key pos_key) const {
    const Key k = key.load(std::memory_order_relaxed);
    const U64 d = data.load(std::memory_order_relaxed);
    return pos_key == (k ^ d);
  }
  constexpr void clear();

  TTEntry read(Ply ply) const;
  void    write(Key pos_key, U8 age, Depth depth, Ply ply, TTBound bound, Move move, Eval eval,
                Eval value);

private:
  constexpr void save(TTEntry tt);
};

class TT {
  PackedTTEntry *table_ = nullptr;
  U64            n_entries_;
  U8             age_;

  size_t index(Key key) const { return (U128(key) * U128(n_entries_)) >> 64; }

public:
  TT(size_t mb);
  ~TT();

  U8   age() const { return age_; }
  void reset_age() { age_ = 0; }
  void incr_age();

  void   resize(size_t mb);
  void   clear();
  size_t size() const { return n_entries_; }
  size_t hashfull() const;

  std::pair<bool, PackedTTEntry &> probe(Key key);
};

} // namespace Lyra
