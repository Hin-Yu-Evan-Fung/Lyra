#include "network.hpp"

#include "incbin.hpp"
#include "nnue.hpp"
#include "utils.hpp"

#include <cassert>
#include <cstring>

namespace Lyra::NNUE {

Network network;

INCBIN(NetworkRaw, NETWORK_PATH);

void load_network() {
  ALWAYS_ASSERT(gNetworkRawSize == sizeof(Network));
  std::memcpy(&network, gNetworkRawData, sizeof(Network));
  nnue.init();
}

} // namespace Lyra::NNUE
