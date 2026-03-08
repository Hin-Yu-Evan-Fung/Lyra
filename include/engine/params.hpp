#pragma once

#include "core/defs.hpp"

#include <string_view>

namespace Lyra {

/******************************************\
|==========================================|
|                Constants                 |
|==========================================|
\******************************************/

// Engine config
constexpr std::string_view NAME    = "Lyra";
constexpr std::string_view AUTHOR  = "Evan Fung";
constexpr std::string_view VERSION = "1.0";

constexpr size_t TTSize       = 32;
constexpr size_t NThreads     = 1;
constexpr int    MoveOverhead = 300;
constexpr U64    ClockFreq    = 2048;

// General constants
constexpr Ply Rule50Ply = 100;
constexpr Ply MaxPly    = 2048;
constexpr U16 MaxMoves  = 256;

// General search constants
constexpr Depth MaxDepth    = 256;
constexpr Depth DepthQS     = 0;
constexpr Depth StackOffset = 4;
constexpr Depth AspWinDepth = 5;
constexpr Eval  AspWinDelta = 15;

// Main search constants
constexpr U16   HistBufSize              = 32;
constexpr Depth RFPDepth                 = 8;
constexpr Eval  RFPFactor                = 100;
constexpr Eval  RazorConstFactor         = 500;
constexpr Eval  RazorQuadFactor          = 300;
constexpr Eval  SeePieceVals[NPieceType] = {170, 440, 460, 710, 1320, 0};

// Eval constants
constexpr Eval EvalInf       = 30000;
constexpr Eval EvalNone      = -EvalInf - 1;
constexpr Eval EvalMate      = 29000;
constexpr Eval EvalMateBound = EvalMate - MaxDepth;
constexpr Eval EvalDraw      = 0;
constexpr Eval EvalStop      = 0;

// History Constant
constexpr Eval HistMax  = 16384;
constexpr U16  ContSize = 4;

} // namespace Lyra
