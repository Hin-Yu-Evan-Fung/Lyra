#include "defs.hpp"

#include <string_view>

namespace Lyra {

constexpr std::string_view NAME    = "Lyra";
constexpr std::string_view AUTHOR  = "Evan Fung";
constexpr std::string_view VERSION = "1.0";

constexpr size_t TT_SIZE       = 32;
constexpr size_t THREADS       = 1;
constexpr int    MOVE_OVERHEAD = 300;
constexpr U64    CLOCK_FREQ    = 2048;

constexpr Depth  MaxDepth      = 256;
constexpr Ply    MaxPly        = 2048;
constexpr size_t MaxMoves      = 256;
constexpr Ply    Rule50Ply     = 100;
constexpr Eval   EvalInf       = 30000;
constexpr Eval   EvalMate      = 29000;
constexpr Eval   EvalMateBound = EvalMate - MaxDepth;
constexpr Eval   EvalDraw      = 0;
constexpr Eval   EvalStop      = 0;

constexpr Depth DepthQS     = 0;
constexpr Depth StackOffset = 2;

constexpr Eval HistMax = 16384;

} // namespace Lyra
