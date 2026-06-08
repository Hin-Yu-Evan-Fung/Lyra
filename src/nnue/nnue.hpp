#pragma once

#include "accumulator.hpp"
#include "bitboard.hpp"
#include "defs.hpp"
#include "nnue_params.hpp"
#include "nnue_utils.hpp"

namespace Lyra::NNUE {

/******************************************\
|==========================================|
|              NNUE Evaluation             |
|==========================================|
\******************************************/

struct NNUE {
  NDArray<Accumulator, 2 * NInBuckets, 2 * NInBuckets> table;

  void init() {
    for (unsigned i = 0; i < 2 * NInBuckets; ++i) {
      for (unsigned j = 0; j < 2 * NInBuckets; ++j) {
        table[i][j].white = network.ft_biases;
        table[i][j].black = network.ft_biases;
      }
    }
  }

  Eval evaluate(const Board &board) {
    Square wksq = board.ksq<White>();
    Square bksq = board.ksq<Black>();

    size_t w_bucket   = input_bucket_index<White>(wksq);
    size_t b_bucket   = input_bucket_index<Black>(bksq);
    size_t out_bucket = output_bucket_index(popcount(board.bb()));

    Accumulator &acc = table[w_bucket][b_bucket];

    acc.update(board);
    return acc.propagate(board.stm(), out_bucket);
  }
};

inline NNUE nnue;

} // namespace Lyra::NNUE
