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

static constexpr size_t NInBuckets  = 8;
static constexpr size_t NOutBuckets = 8;

static constexpr size_t NFeatures = (size_t)NSquare * (size_t)NPiece;
static constexpr size_t L1        = 1024;

// clang-format off
constexpr size_t BucketMap[NSquare] = {
    0,  1,  2,  3, 11, 10,  9,  8,
    4,  4,  5,  5, 13, 13, 12, 12,
    6,  6,  6,  6, 14, 14, 14, 14,
    7,  7,  7,  7, 15, 15, 15, 15,
    7,  7,  7,  7, 15, 15, 15, 15,
    7,  7,  7,  7, 15, 15, 15, 15,
    7,  7,  7,  7, 15, 15, 15, 15,
    7,  7,  7,  7, 15, 15, 15, 15,
};
// clang-format on

} // namespace Lyra::NNUE
