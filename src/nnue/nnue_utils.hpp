#pragma once

#include "defs.hpp"
#include "nnue_params.hpp"

#include <algorithm>
#include <array>
#include <cstddef>

namespace Lyra::NNUE {

/******************************************\
|==========================================|
|             Align64 Definition           |
|==========================================|
\******************************************/

template <typename T, size_t N>
struct alignas(64) Align64 : std::array<T, N> {};

using SideAccumulator = Align64<I16, L1>;

/******************************************\
|==========================================|
|           Square Clipped Relu            |
|==========================================|
\******************************************/

constexpr I32 screlu(I16 x) {
  I32 c = std::clamp((I32)x, 0, QA);
  return c * c;
}

/******************************************\
|==========================================|
|             Index functions              |
|==========================================|
\******************************************/

template <Colour C>
constexpr size_t input_bucket_index(Square ksq) {
  return BucketMap[relative_sq(C, ksq)];
}

constexpr size_t output_bucket_index(size_t num_pieces) {
  return (num_pieces - 2) * NOutBuckets / 32;
}

template <Colour C>
constexpr size_t feature_index(Colour c, PieceType pt, Square sq, Square ksq) {
  constexpr size_t BStride = 768;
  constexpr size_t CStride = 384;
  constexpr size_t PStride = 64;

  Square rsq = relative_sq(C, sq);

  if (file_of(ksq) > FileD) {
    ksq = flip_file(ksq);
    rsq = flip_file(rsq);
  }

  size_t bucket = input_bucket_index<C>(ksq);
  return bucket * BStride + (c ^ C) * CStride + pt * PStride + rsq;
}

} // namespace Lyra::NNUE
