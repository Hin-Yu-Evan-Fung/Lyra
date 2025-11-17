#pragma once

#include <string>

#include "defs.hpp"
#include "engine.hpp"

namespace Lyra {

/******************************************\
|==========================================|
|              UCI interface               |
|==========================================|
\******************************************/

struct UCI {
public:
    UCI();

    void loop();

private:
    Engine engine;

    void parse_go(std::istringstream& is);
    void parse_pos(std::istringstream& is);
};

}  // namespace Lyra
