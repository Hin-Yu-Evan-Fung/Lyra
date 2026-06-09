#include "tt.hpp"

#include "defs.hpp"
#include "move.hpp"
#include "params.hpp"
#include "utils.hpp"

#include <cassert>
#include <cstdlib>

namespace Lyra {

/******************************************\
|==========================================|
|           Transposition Table            |
|==========================================|
\******************************************/

void TT::resize(size_t mb) {
  n_entries_ = calc_no_of_entries(mb);

  delete[] entries_;
  entries_ = new TTEntry[n_entries_];
}

void TT::clear() {
  age8_ = 0;
  for (size_t i = 0; i < n_entries(); ++i) {
    entries_[i] = {};
  }
}

size_t TT::hashfull() const {
  size_t count = 0;
  for (size_t i = 0; i < 1000U; ++i) {
    const TTEntry &entry = entries_[i];
    count += (entry.key16 != 0 && !entry.relative_age(age8_));
  }
  return count;
}

/******************************************\
|==========================================|
|                 Read/Write               |
|==========================================|
\******************************************/

TTData TTEntry::read(Ply p) const {
  return {
      Bound((age_bound8 & BoundMask) >> BoundShift),
      Depth(depth8 + TTDepthOff),
      Move(move16),
      Eval(eval16),
      from_TT(Eval(value16), p),
  };
}

void TTEntry::save(Key k, Age curr, Bound b, Depth d, Ply p, Move m, Eval ev, Eval v) {
  // Preserve old tt move if we don't have a new one
  if (m || U16(k) != key16) {
    move16 = m;
  }

  // Overwrite less valuable entries
  if (b == Bound::Exact || U16(k) != key16 || relative_age(curr) || d - TTDepthOff > depth8) {
    key16      = U16(k);
    depth8     = U8(d - TTDepthOff);
    age_bound8 = Age(curr | b << BoundShift);
    eval16     = I16(ev);
    value16    = I16(to_TT(v, p));
  }
}

/******************************************\
|==========================================|
|             TT Read / Write              |
|==========================================|
\******************************************/

void TT::prefetch(Key key) const { __builtin_prefetch(&entries_[index(key)]); }

TTResult TT::probe(Key key, Ply ply) {
  TTEntry *const tte   = &entries_[index(key)];
  const U16      key16 = U16(key);

  if (tte->key16 == key16) {
    tt_hits_++;
    return {tte->is_occupied(), tte, tte->read(ply)};
  }

  tt_collisions_++;
  return {false, tte, TTData{Bound::None, TTDepthOff, NoMove, EvalInvalid, EvalInvalid}};
}

} // namespace Lyra
