#include "tt.hpp"

#include "defs.hpp"
#include "move.hpp"
#include "utils.hpp"

#include <atomic>
#include <cassert>

namespace Lyra {

/******************************************\
|==========================================|
|           Transposition Table            |
|==========================================|
\******************************************/

void TT::resize(size_t mb) {
  size_t target = calc_no_of_entries(mb);
  entries_.resize(target);
}

void TT::clear() {
  for (size_t i = 0; i < size(); i++) entries_[i].clear();
}

size_t TT::hashfull() const {
  size_t count = 0;
  for (int i = 0; i < 1000; i++) {
    const TTEntry &entry = entries_[i].unpack(0);
    count += (entry.key != 0 && entry.age == age_);
  }
  return count;
}

/******************************************\
|==========================================|
|            PackedTTEntry Utils           |
|==========================================|
\******************************************/

void PackedTTEntry::pack(const TTEntry &e, Ply ply) {
  U8 depth = std::clamp(e.depth, (Depth)0, (Depth)127);

  U64 d = U64(e.age) & AgeMask;
  d |= (U64(depth) << 7) & DepthMask;
  d |= (U64(e.bound) << 14) & BoundMask;
  d |= (U64(e.move) << 16) & MoveMask;
  d |= (U64(U16(e.eval)) << 32) & EvalMask;
  d |= (U64(U16(to_TT(e.value, ply))) << 48) & ValueMask;

  key.store(e.key ^ d, std::memory_order_relaxed);
  data.store(d, std::memory_order_relaxed);
}

TTEntry PackedTTEntry::unpack(Ply ply) const {
  U64 k       = key.load(std::memory_order_relaxed);
  U64 d       = data.load(std::memory_order_relaxed);
  Key pos_key = k ^ d;

  return {
      pos_key,
      U8(d & AgeMask),
      Depth((d & DepthMask) >> 7),
      Bound((d & BoundMask) >> 14),
      Move((d & MoveMask) >> 16),
      I16((d & EvalMask) >> 32),
      from_TT(I16((d & ValueMask) >> 48), ply),
  };
}

/******************************************\
|==========================================|
|             TT Read / Write              |
|==========================================|
\******************************************/

void TT::prefetch(Key key) const { __builtin_prefetch(&entries_[index(key)]); }

std::pair<bool, TTEntry> TT::read(Key key, Ply ply) {
  const size_t         index = this->index(key);
  const PackedTTEntry &pe    = entries_[index];

  const Key pe_key  = pe.key.load(std::memory_order_relaxed);
  const U64 pe_data = pe.data.load(std::memory_order_relaxed);

  if ((pe_key ^ key) == pe_data) {
    return {true, pe.unpack(ply)};
  } else {
    return {false, {}};
  }
}

void TT::write(Key key, Depth depth, Ply ply, Bound bound, Move move, Eval eval, Eval value) {
  const size_t   index = this->index(key);
  PackedTTEntry &pe    = entries_[index];
  const TTEntry &old   = pe.unpack(ply);

  if (old.age != age_ || old.key != key || bound == Bound::Exact || old.depth < depth) {
    Move    tt_move = (key == old.key && move == NoMove) ? old.move : move;
    TTEntry e{
        key, age_, depth, bound, tt_move, eval, value,
    };
    pe.pack(std::move(e), ply);
  }
}

} // namespace Lyra
