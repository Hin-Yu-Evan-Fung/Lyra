#pragma once

#include <string>

#include "defs.hpp"
#include "engine.hpp"

namespace Lyra {

/******************************************\
|==========================================|
|              Engine Config               |
|==========================================|
\******************************************/

constexpr std::string_view NAME    = "Lyra";
constexpr std::string_view AUTHOR  = "Evan Fung";
constexpr std::string_view VERSION = "1.0";

constexpr size_t HASH_SIZE         = 32;
constexpr size_t THREADS           = 1;
constexpr int    MOVE_OVERHEAD     = 300;

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
