#pragma once

#include "defs.hpp"
#include "history.hpp"

#include <cstddef>

namespace Lyra {

struct PVLine {
  Move   moves[MaxDepth];
  size_t length;

  void        update(const PVLine &other, Move best);
  void        clear() { length = 0; }
  std::string format(bool chess960) const;
};

struct StackEntry {
  Killer killer;
  PVLine pv;
  U16    ply;
};

} // namespace Lyra
