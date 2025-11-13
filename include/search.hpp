#pragma once

#include "defs.hpp"
#include "utils.hpp"

namespace Lyra {

struct SearchConfig {
    Time     time[NColour];
    Time     inc[NColour];
    Depth    depth;
    unsigned moves_to_go;
    Time     move_time;
    bool     is_infinite;
};

}  // namespace Lyra
