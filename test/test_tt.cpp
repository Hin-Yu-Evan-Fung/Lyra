#include "move.hpp"
#include "params.hpp"
#include "tt.hpp"
#include "utils.hpp"

#include <gtest/gtest.h>

namespace Lyra {

TEST(tt, depth_replacement) {
  TT tt(1);

  Depth max_depth = 0;
  for (unsigned i = 0; i < 1000; ++i) {
    Depth depth                 = random() % 128;
    max_depth                   = std::max(depth, max_depth);
    auto [tt_hit, tte, tt_data] = tt.probe(0x123, 127);
    tte->save(0x123, tt.age(), Bound::Lower, depth, 127, NullMove, EvalInvalid, mate_in(5));
  }

  auto [tt_hit, tte, tt_data] = tt.probe(0x123, 127);
  ASSERT_EQ(tt_data.depth, max_depth);
}

TEST(tt, bound_replacement) {
  TT tt(1);

  {
    auto [tt_hit, tte, tt_data] = tt.probe(0x123, 5);

    tte->save(0x123, tt.age(), Bound::Lower, 127, 127, NullMove, EvalInvalid, mate_in(5));
    tte->save(0x123, tt.age(), Bound::Exact, 127, 127, NullMove, EvalInvalid, mate_in(5));
    tte->save(0x123, tt.age(), Bound::Lower, 127, 127, NullMove, EvalInvalid, mated_in(5));
  }

  auto [tt_hit, tte, tt_data] = tt.probe(0x123, 5);
  ASSERT_EQ(tt_data.bound, Bound::Exact);
}

TEST(tt, age_replacement) {
  TT tt(1);

  {
    auto [tt_hit, tte, tt_data] = tt.probe(0x123, 5);

    tte->save(0x123, tt.age(), Bound::Lower, 5, 5, NullMove, EvalMateBound, mate_in(5));
    tt.incr_age();
    tte->save(0x123, tt.age(), Bound::Lower, 4, 5, NullMove, EvalMateBound, mated_in(5));
    tte->save(0x123, tt.age(), Bound::Lower, 4, 5, NullMove, EvalMateBound, mate_in(5));
  }

  auto [tt_hit, tte, tt_data] = tt.probe(0x123, 5);
  ASSERT_EQ(tt_data.depth, 4);
  ASSERT_EQ(tt_data.bound, Bound::Lower);
  ASSERT_EQ(tt_data.move, NullMove);
  ASSERT_EQ(tt_data.eval, EvalMateBound);
  ASSERT_EQ(tt_data.value, mated_in(5));
}

TEST(tt, tt_resize) {
  TT tt(16);
  ASSERT_EQ(tt.n_entries(), (16UL << 20) / sizeof(TTEntry));
  tt.resize(32);
  ASSERT_EQ(tt.n_entries(), (32UL << 20) / sizeof(TTEntry));
  tt.resize(17);
  ASSERT_EQ(tt.n_entries(), (17UL << 20) / sizeof(TTEntry));
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
