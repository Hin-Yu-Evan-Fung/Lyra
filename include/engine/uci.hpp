#pragma once

#include "engine.hpp"

#include <sstream>

namespace Lyra {

/******************************************\
|==========================================|
|              UCI interface               |
|==========================================|
\******************************************/

class UCI {
public:
  void loop();

private:
  Engine engine_;

  void parse_go(std::istringstream &is);
  void parse_pos(std::istringstream &is);
  void parse_perft(std::istringstream &is);
  void parse_option(std::istringstream &is);
};

} // namespace Lyra
