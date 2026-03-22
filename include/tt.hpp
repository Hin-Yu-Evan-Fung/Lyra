#pragma once

#include "defs.hpp"

#include <atomic>
#include <utility>
#include <vector>

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
  Lower, // Fail high nodes, lower bound for position eval
  Upper  // Fail low nodes, upper bound for position eval
};

struct TTEntry {
  Key     key;
  Age     age;
  Depth   depth;
  TTBound bound;
  Move    move;
  Eval    eval;
  Eval    value;
};

struct PackedTTEntry {
  std::atomic<U64> key;
  std::atomic<U64> data;

  PackedTTEntry()
      : key(0)
      , data(0) {}
  PackedTTEntry(const PackedTTEntry &pe)
      : key(pe.key.load(std::memory_order_relaxed))
      , data(pe.data.load(std::memory_order_relaxed)) {}

  void clear() {
    key.store(0, std::memory_order_relaxed);
    data.store(0, std::memory_order_relaxed);
  }
  void    pack(const TTEntry &e, Ply ply);
  TTEntry unpack(Ply ply) const;
};

class TT {
  std::vector<PackedTTEntry> entries_;
  Age                        age_;

  constexpr size_t calc_no_of_entries(size_t mb) const {
    return (mb << 20) / sizeof(PackedTTEntry);
  }
  constexpr size_t index(Key key) const { return (U128(key) * U128(size())) >> 64; }

public:
  TT(size_t mb)
      : age_(0) {
    resize(mb);
    clear();
  }

  constexpr Age    age() const { return age_; }
  constexpr size_t size() const { return entries_.size(); }
  size_t           hashfull() const;

  void reset_age() { age_ = 0; }
  void incr_age() { age_ = (age_ + 1) & 0x7FUL; }
  void resize(size_t mb);
  void clear();

  std::pair<bool, TTEntry> read(Key key, Ply ply);
  void write(Key key, Depth depth, Ply ply, TTBound bound, Move move, Eval eval, Eval value);
};

} // namespace Lyra
