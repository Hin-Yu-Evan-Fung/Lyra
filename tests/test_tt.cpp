#include "defs.hpp"
#include "move.hpp"
#include "tt.hpp"
#include <gtest/gtest.h>

namespace Lyra {

TEST(tt, basic_read_write) {
  PackedTTEntry entry;
  entry.write(0x123, 0x1, 5, 5, TTBound::Exact, NullMove, 0, 1);

  TTEntry ttentry = entry.read(5);

  ASSERT_EQ(ttentry.age, 0x1);
  ASSERT_EQ(ttentry.depth, 5);
  ASSERT_EQ(ttentry.bound, TTBound::Exact);
  ASSERT_EQ(ttentry.move, NullMove);
  ASSERT_EQ(ttentry.eval, 0);
  ASSERT_EQ(ttentry.value, 1);
}

TEST(tt, mate_value) {
  PackedTTEntry entry;
  entry.write(0x123, 0x1, 5, 5, TTBound::Exact, NullMove, EvalMateBound,
              EvalMate);

  TTEntry ttentry = entry.read(5);

  ASSERT_EQ(ttentry.age, 0x1);
  ASSERT_EQ(ttentry.depth, 5);
  ASSERT_EQ(ttentry.bound, TTBound::Exact);
  ASSERT_EQ(ttentry.move, NullMove);
  ASSERT_EQ(ttentry.eval, EvalMateBound);
  ASSERT_EQ(ttentry.value, EvalMate);
}

TEST(tt, depth_replacement) {
  PackedTTEntry entry;

  entry.write(0x123, 0x1, 5, 5, TTBound::Lower, NullMove, EvalMateBound,
              EvalMate);
  entry.write(0x123, 0x1, 6, 5, TTBound::Lower, NullMove, EvalMateBound,
              EvalMate);
  entry.write(0x123, 0x1, 4, 5, TTBound::Lower, NullMove, EvalMateBound,
              EvalMate);

  TTEntry ttentry = entry.read(5);
  ASSERT_EQ(ttentry.depth, 6);
}

TEST(tt, bound_replacement) {
  PackedTTEntry entry;

  entry.write(0x123, 0x1, 5, 5, TTBound::Lower, NullMove, EvalMateBound,
              EvalMate);
  entry.write(0x123, 0x1, 4, 5, TTBound::Exact, NullMove, EvalMateBound,
              EvalMate);
  entry.write(0x123, 0x1, 4, 5, TTBound::Lower, NullMove, EvalMateBound,
              EvalMate);

  TTEntry ttentry = entry.read(5);
  ASSERT_EQ(ttentry.bound, TTBound::Exact);
}

TEST(tt, age_replacement) {
  PackedTTEntry entry;

  entry.write(0x123, 0x1, 5, 5, TTBound::Lower, NullMove, EvalMateBound,
              EvalMate);
  entry.write(0x123, 0x2, 4, 5, TTBound::Lower, NullMove, EvalMateBound,
              EvalMate);
  entry.write(0x123, 0x2, 4, 5, TTBound::Lower, NullMove, EvalMateBound,
              EvalMate);

  TTEntry ttentry = entry.read(5);
  ASSERT_EQ(ttentry.age, 0x2);
  ASSERT_EQ(ttentry.depth, 4);
  ASSERT_EQ(ttentry.bound, TTBound::Lower);
  ASSERT_EQ(ttentry.move, NullMove);
  ASSERT_EQ(ttentry.eval, EvalMateBound);
  ASSERT_EQ(ttentry.value, EvalMate);
}

TEST(tt, key_replacement) {
  PackedTTEntry entry;

  entry.write(0x123, 0x1, 5, 5, TTBound::Lower, NullMove, EvalMateBound,
              EvalMate);
  entry.write(0x12, 0x1, 4, 5, TTBound::Lower, NullMove, EvalMateBound,
              EvalMate);
  entry.write(0x12, 0x1, 4, 5, TTBound::Lower, NullMove, EvalMateBound,
              EvalMate);

  TTEntry ttentry = entry.read(5);
  ASSERT_EQ(ttentry.age, 0x1);
  ASSERT_EQ(ttentry.depth, 4);
  ASSERT_EQ(ttentry.bound, TTBound::Lower);
  ASSERT_EQ(ttentry.move, NullMove);
  ASSERT_EQ(ttentry.eval, EvalMateBound);
  ASSERT_EQ(ttentry.value, EvalMate);
}

TEST(tt, validity) {
  PackedTTEntry entry;

  entry.write(0x123, 0x1, 5, 5, TTBound::Lower, NullMove, EvalMateBound,
              EvalMate);

  ASSERT_TRUE(entry.is_valid(0x123));
  ASSERT_FALSE(entry.is_valid(0x12));
}

TEST(tt, probe) {
  TT tt(16);

  auto [tt_hit, tt_entry] = tt.probe(0x123);

  tt_entry.write(0x123, 0x1, 5, 5, TTBound::Lower, NullMove, EvalMateBound,
                 EvalMate);

  auto [tt_hit_2, tt_entry_2] = tt.probe(0x123);

  ASSERT_TRUE(tt_hit_2);

  TTEntry ttentry = tt_entry_2.read(5);
  ASSERT_EQ(ttentry.age, 0x1);
  ASSERT_EQ(ttentry.depth, 5);
  ASSERT_EQ(ttentry.bound, TTBound::Lower);
  ASSERT_EQ(ttentry.move, NullMove);
  ASSERT_EQ(ttentry.eval, EvalMateBound);
  ASSERT_EQ(ttentry.value, EvalMate);
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
