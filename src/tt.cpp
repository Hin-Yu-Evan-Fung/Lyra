#include "tt.hpp"

#include "defs.hpp"
#include "utils.hpp"

#include <atomic>
#include <cassert>
#include <new>

namespace Lyra {

/******************************************\
|==========================================|
|            PackedTTEntry Utils           |
|==========================================|
\******************************************/

constexpr U64 AgeMask   = 0x7FUL;
constexpr U64 DepthMask = 0x7FUL << 7;
constexpr U64 BoundMask = 0x3UL << 14;
constexpr U64 MoveMask  = 0xFFFFUL << 16;
constexpr U64 EvalMask  = 0xFFFFUL << 32;
constexpr U64 ValueMask = 0xFFFFUL << 48;

constexpr void PackedTTEntry::clear() {
  key.store(0, std::memory_order_relaxed);
  data.store(0, std::memory_order_relaxed);
}

TTEntry PackedTTEntry::read(Ply ply) const {
  U64 k       = key.load(std::memory_order_relaxed);
  U64 d       = data.load(std::memory_order_relaxed);
  Key pos_key = k ^ d;

  return {
      pos_key,
      U8(d & AgeMask),
      Depth((d & DepthMask) >> 7),
      TTBound((d & BoundMask) >> 14),
      Move((d & MoveMask) >> 16),
      I16((d & EvalMask) >> 32),
      EvalUtils::from_TT(I16((d & ValueMask) >> 48), ply),
  };
}

constexpr void PackedTTEntry::save(TTEntry e) {
  U64 d = U64(e.age) & AgeMask;
  d |= (U64(e.depth) << 7) & DepthMask;
  d |= (U64(e.bound) << 14) & BoundMask;
  d |= (U64(e.move) << 16) & MoveMask;
  d |= (U64(U16(e.eval)) << 32) & EvalMask;
  d |= (U64(U16(e.value)) << 48) & ValueMask;

  key.store(e.key ^ d, std::memory_order_relaxed);
  data.store(d, std::memory_order_relaxed);
}

void PackedTTEntry::write(Key pos_key, U8 age, Depth depth, Ply ply, TTBound bound, Move move,
                          Eval eval, Eval value) {
  const TTEntry &old = read(ply);
  // Replace strategy
  if (!(age != old.age || pos_key != old.key || bound == Exact || depth > old.depth)) return;

  const Move new_move = move || pos_key != old.key ? move : old.move;
  save({pos_key, age, depth, bound, new_move, eval, EvalUtils::to_TT(value, ply)});
}

/******************************************\
|==========================================|
|           Transposition Table            |
|==========================================|
\******************************************/

TT::TT(size_t mb)
    : age_(0) {
  resize(mb);
}
TT::~TT() { delete[] table_; }

void TT::incr_age() { age_ = (age_ + 1) & AgeMask; }

void TT::resize(size_t mb) {
  if (table_ != nullptr) delete[] table_;

  constexpr size_t entry_size = sizeof(PackedTTEntry);
  const size_t     n_entries  = (mb << 20) / entry_size;

  table_     = new PackedTTEntry[n_entries];
  n_entries_ = n_entries;

  if (table_ == nullptr) throw std::bad_alloc();
}

void TT::clear() {
  for (size_t i = 0; i < size(); i++) table_[i].clear();
}

size_t TT::hashfull() const {
  size_t count = 0;
  for (int i = 0; i < 1000; i++) {
    const TTEntry &entry = table_[i].read(0);
    count += (entry.key != 0 && entry.age == age_);
  }
  return count;
}

std::pair<bool, PackedTTEntry &> TT::probe(Key key) {
  PackedTTEntry &entry = table_[index(key)];
  return {entry.is_valid(key), entry};
}

} // namespace Lyra
