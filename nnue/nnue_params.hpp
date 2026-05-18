#pragma once

#include "defs.hpp"

#include <cstddef>

namespace Lyra::NNUE {

/******************************************\
|==========================================|
|            Quantisation Params           |
|==========================================|
\******************************************/

static constexpr I32 QA    = 255;
static constexpr I32 QB    = 64;
static constexpr I32 QAB   = QA * QB;
static constexpr I32 SCALE = 400;

/******************************************\
|==========================================|
|              Network Params              |
|==========================================|
\******************************************/

static constexpr size_t NInBuckets  = 4;
static constexpr size_t NOutBuckets = 8;

static constexpr size_t NFeatures = 768;
static constexpr size_t L1        = 512;

// clang-format off
constexpr size_t BucketMap[64] = {
    0,  0,  1,  1,  5,  5,  4,  4,
    2,  2,  2,  2,  6,  6,  6,  6,
    2,  2,  2,  2,  6,  6,  6,  6,
    3,  3,  3,  3,  7,  7,  7,  7,
    3,  3,  3,  3,  7,  7,  7,  7,
    3,  3,  3,  3,  7,  7,  7,  7,
    3,  3,  3,  3,  7,  7,  7,  7,
    3,  3,  3,  3,  7,  7,  7,  7,
};
// clang-format on

} // namespace Lyra::NNUE
