#pragma once

#include "defs.hpp"
#include "nnue_params.hpp"
#include "nnue_utils.hpp"

namespace Lyra::NNUE {

/******************************************\
|==========================================|
|                NNUE Weights              |
|==========================================|
\******************************************/

struct Network {
  Align64<I16, L1 * NFeatures * NInBuckets> ft_weights;
  Align64<I16, L1>                          ft_biases;
  Align64<I16, L1>                          out_weights[NOutBuckets][2];
  Align64<I16, NOutBuckets>                 out_bias;
};

extern Network network;

void load_network(const uint8_t *data, size_t size);

} // namespace Lyra::NNUE
