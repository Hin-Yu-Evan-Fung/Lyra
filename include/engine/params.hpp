#pragma once

#include "core/defs.hpp"

namespace Lyra {

/******************************************\
|==========================================|
|                Constants                 |
|==========================================|
\******************************************/

// General constants
constexpr Ply Rule50Ply = 100;
constexpr Ply MaxPly = 2048;
constexpr U16 MaxMoves = 256;

// Search constants
constexpr Depth MaxDepth = 256;
constexpr Depth DepthQS = 0;
constexpr Depth StackOffset = 4;

// Eval constants
constexpr Eval EvalInf = 30000;
constexpr Eval EvalNone = -EvalInf - 1;
constexpr Eval EvalMate = 29000;
constexpr Eval EvalMateBound = EvalMate - MaxDepth;
constexpr Eval EvalDraw = 0;
constexpr Eval EvalStop = 0;

// History Constant
constexpr Eval HistMax = 16384;
constexpr U16 ContSize = 4;

} // namespace Lyra
