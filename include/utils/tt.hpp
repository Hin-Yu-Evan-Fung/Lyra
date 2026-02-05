#pragma once

#include <atomic>
#include <utility>

#include "core/defs.hpp"

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
  Exact, // PV nodes
  Upper, // Fail low nodes, upper bound for position eval
  Lower, // Fail high nodes, lower bound for position eval
};

struct TTEntry {
  Key key;
  U8 age;
  Depth depth;
  TTBound bound;
  Move move;
  Eval eval;
  Eval value;
};

struct PackedTTEntry {
  std::atomic_uint64_t key;
  std::atomic_uint64_t data;

  constexpr bool is_valid(Key pos_key) const { return (pos_key ^ key) == data; }
  constexpr void clear();

  TTEntry read(Ply ply) const;
  void write(Key pos_key, U8 age, Depth depth, Ply ply, TTBound bound,
             Move move, Eval eval, Eval value);

private:
  constexpr void save(TTEntry tt);
};

class TT {
  PackedTTEntry *table_ = nullptr;
  U64 hash_mask_;
  U8 age_;

  size_t index(Key key) const { return key & hash_mask_; }

public:
  TT(size_t mb);
  ~TT();

  U8 age() const { return age_; }
  void reset_age() { age_ = 0; }
  void incr_age();

  void resize(size_t mb);
  void clear();
  size_t size() const { return hash_mask_ + 1; }
  size_t hashfull() const;

  std::pair<bool, PackedTTEntry &> probe(Key key);
};

} // namespace Lyra
