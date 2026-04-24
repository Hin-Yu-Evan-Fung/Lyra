#pragma once

#include "defs.hpp"

#include <string_view>

namespace Lyra {

static constexpr std::string_view NAME    = "Lyra";
static constexpr std::string_view AUTHOR  = "Evan Fung";
static constexpr std::string_view VERSION = "1.0";

static constexpr size_t TT_SIZE       = 32;
static constexpr size_t THREADS       = 1;
static constexpr int    MOVE_OVERHEAD = 50;
static constexpr U64    CLOCK_FREQ    = 2048;

// General constants
static constexpr Depth  MaxDepth  = 256;
static constexpr Ply    MaxPly    = 2048;
static constexpr size_t MaxMoves  = 256;
static constexpr Ply    Rule50Ply = 100;

// Hash table constants
static constexpr size_t NBuckets  = 4;
static constexpr U64    AgeMask   = 0x7FUL;
static constexpr U64    DepthMask = 0x7FUL << 7;
static constexpr U64    BoundMask = 0x3UL << 14;
static constexpr U64    MoveMask  = 0xFFFFUL << 16;
static constexpr U64    EvalMask  = 0xFFFFUL << 32;
static constexpr U64    ValueMask = 0xFFFFUL << 48;

// History Constant
static constexpr Eval HistMax  = 16384;
static constexpr Eval ContSize = 2;

// General search constants
static constexpr Depth DepthQS     = 0;
static constexpr Depth StackOffset = ContSize;

// SEE values
static constexpr Eval SeePieceVals[NPieceType] = {170, 440, 460, 710, 1320, 0};

// Eval constants
static constexpr Eval EvalInf       = 30000;
static constexpr Eval EvalInvalid   = -EvalInf - 1;
static constexpr Eval EvalMate      = 29000;
static constexpr Eval EvalMateBound = EvalMate - MaxDepth;
static constexpr Eval EvalDraw      = 0;
static constexpr Eval EvalStop      = 0;

}; // namespace Lyra
