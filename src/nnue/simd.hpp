#pragma once

#include "nnue_utils.hpp"

#ifdef __AVX2__
#include <emmintrin.h>
#include <immintrin.h>
#define USE_AVX2
#endif

namespace Lyra::NNUE {

#ifdef USE_AVX2
inline __m256i load_i16s(const SideAccumulator &acc, size_t start_idx) {
  return _mm256_load_si256(reinterpret_cast<const __m256i *>(&acc[start_idx]));
}

#endif

#ifdef USE_AVX2
inline I32 horizontal_sum_i32(__m256i sum) {
  __m128i low_128  = _mm256_castsi256_si128(sum);          // [0, 1, 2, 3]
  __m128i high_128 = _mm256_extracti128_si256(sum, 1);     // [4, 5, 6, 7]
  __m128i sum_128  = _mm_add_epi32(low_128, high_128);     // [0+4, 1+5, 2+6, 3+7]
  __m128i upper_64 = _mm_unpackhi_epi64(sum_128, sum_128); // [2+6, 3+7, 2+6, 3+7]
  __m128i sum_64   = _mm_add_epi32(upper_64, sum_128);     // [0+2+4+6, 1+3+5+7, ...]
  __m128i upper_32 = _mm_shuffle_epi32(sum_64, 1);         // [1+5+3+7. ...]
  __m128i sum_32   = _mm_add_epi32(upper_32, sum_64);      // [0+1+2+3+4+5+6+7, ...]

  return _mm_cvtsi128_si32(sum_32);
}

inline I32 dotprod(const SideAccumulator &acc, const SideAccumulator &weights) {
  __m256i sum = _mm256_setzero_si256();
  __m256i min = _mm256_setzero_si256();
  __m256i max = _mm256_set1_epi16(QA);

  for (unsigned i = 0; i < L1 / 16; ++i) {
    __m256i v       = load_i16s(acc, i * 16);
    __m256i w       = load_i16s(weights, i * 16);
    v               = _mm256_min_epi16(_mm256_max_epi16(v, min), max);
    __m256i product = _mm256_madd_epi16(v, _mm256_mullo_epi16(v, w));
    sum             = _mm256_add_epi32(sum, product);
  }

  return horizontal_sum_i32(sum);
}

#else
inline I32 dotprod(const SideAccumulator &acc, const SideAccumulator &weights) {
  I32 sum = 0;
  for (unsigned i = 0; i < L1; ++i) {
    sum += screlu(acc[i]) * (I32)weights[i];
  }
  return sum;
}
#endif

} // namespace Lyra::NNUE
