#include "network.hpp"

#include "nnue.hpp"
#include "utils.hpp"

#include <cassert>
#include <cstring>
#include <iostream>

namespace Lyra::NNUE {

Network network;

void load_network(const uint8_t *data, size_t size) {
  ALWAYS_ASSERT(size == sizeof(Network));

  std::memcpy(&network, data, sizeof(Network));
  nnue.init();
}

} // namespace Lyra::NNUE
