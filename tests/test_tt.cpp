#include "move.hpp"
#include "params.hpp"
#include "tt.hpp"
#include "utils.hpp"

#include <cstdlib>
#include <gtest/gtest.h>

namespace Lyra {

TTEntry pack_unpack(const TTEntry &e, Ply ply) {
  PackedTTEntry pe;
  pe.pack(e, ply);
  return pe.unpack(ply);
}

void check_entry(const TTEntry &a, const TTEntry &b) {
  ASSERT_EQ(a.age, b.age);
  ASSERT_EQ(a.depth, b.depth);
  ASSERT_EQ(a.bound, b.bound);
  ASSERT_EQ(a.move, b.move);
  ASSERT_EQ(a.eval, b.eval);
  ASSERT_EQ(a.value, b.value);
}

TEST(tt, basic_pack_unpack) {
  TTEntry e{0x1234, 127, 127, TTBound::Exact, NullMove, EvalInvalid, mated_in(5)};
  check_entry(e, pack_unpack(e, 5));
}

TEST(tt, depth_replacement) {
  TT tt(1);

  Depth max_depth = 0;
  for (unsigned i = 0; i < 1000; ++i) {
    Depth depth = random() % 128;
    max_depth   = std::max(depth, max_depth);
    tt.write(0x123, 127, depth, TTBound::Lower, NullMove, EvalInvalid, mate_in(5));
  }

  auto [tt_hit, tt_entry] = tt.read(0x123, 127);
  ASSERT_EQ(tt_entry.depth, max_depth);
}

TEST(tt, bound_replacement) {
  TT tt(1);

  tt.write(0x123, 127, 127, TTBound::Lower, NullMove, EvalInvalid, mate_in(5));
  tt.write(0x123, 127, 127, TTBound::Exact, NullMove, EvalInvalid, mate_in(5));
  tt.write(0x123, 127, 127, TTBound::Lower, NullMove, EvalInvalid, mated_in(5));

  auto [tt_hit, tt_entry] = tt.read(0x123, 5);
  ASSERT_EQ(tt_entry.bound, TTBound::Exact);
}

TEST(tt, age_replacement) {
  TT tt(1);

  tt.write(0x123, 5, 5, TTBound::Lower, NullMove, EvalMateBound, mate_in(5));
  tt.incr_age();
  tt.write(0x123, 4, 5, TTBound::Lower, NullMove, EvalMateBound, mated_in(5));
  tt.write(0x123, 4, 5, TTBound::Lower, NullMove, EvalMateBound, mate_in(5));

  auto [tt_hit, tt_entry] = tt.read(0x123, 5);
  ASSERT_EQ(tt_entry.age, 0x1);
  ASSERT_EQ(tt_entry.depth, 4);
  ASSERT_EQ(tt_entry.bound, TTBound::Lower);
  ASSERT_EQ(tt_entry.move, NullMove);
  ASSERT_EQ(tt_entry.eval, EvalMateBound);
  ASSERT_EQ(tt_entry.value, mated_in(5));
}

TEST(tt, tt_resize) {
  TT tt(16);
  ASSERT_EQ(tt.size(), (16UL << 20) / sizeof(PackedTTEntry));
  tt.resize(32);
  ASSERT_EQ(tt.size(), (32UL << 20) / sizeof(PackedTTEntry));
  tt.resize(17);
  ASSERT_EQ(tt.size(), (17UL << 20) / sizeof(PackedTTEntry));
}

TEST(tt, tt_age) {
  TT tt(16);
  ASSERT_EQ(tt.age(), 0);
  tt.incr_age();
  ASSERT_EQ(tt.age(), 1);
  tt.reset_age();
  ASSERT_EQ(tt.age(), 0);
}

} // namespace Lyra
