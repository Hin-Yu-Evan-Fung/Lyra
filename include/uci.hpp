#pragma once

#include <sstream>

#include "engine.hpp"

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

  void parse_go(std::istringstream& is);
  void parse_pos(std::istringstream& is);
  void parse_perft(std::istringstream& is);
};

}  // namespace Lyra
