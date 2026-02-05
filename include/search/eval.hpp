#pragma once

#include "core/defs.hpp"

namespace Lyra::EvalUtils {

extern Score PSQT[NPiece][NSquare];
extern int GamePhaseInc[NPieceType];

void init();

} // namespace Lyra::EvalUtils
