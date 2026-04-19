#pragma once

#include "defs.hpp"

#include <string_view>

#ifdef TUNE
#include <print>
#include <vector>

struct Tunable {
  std::string name;
  int        *ptr;
  int         min, max;
  int         step;
};

class TunableRegistry {
  std::vector<Tunable> entries_;

public:
  static TunableRegistry &instance() {
    static TunableRegistry reg;
    return reg;
  }

  void add(const std::string &name, int *ptr, int min, int max, int step) {
    entries_.push_back({name, ptr, min, max, step});
  }

  void print_options() const {
    for (auto &e : entries_) {
      std::println("option name {} type spin default {} min {} max {}", e.name, *e.ptr, e.min,
                   e.max);
    }
  }

  void print_spsa_params() const {
    for (auto &e : entries_) {
      std::println("{}, int, {}.0, {}.0, {}.0, {}.0, 0.002", e.name, *e.ptr, e.min, e.max, e.step);
    }
  }

  bool set(const std::string &name, int value) {
    for (auto &e : entries_) {
      if (e.name == name) {
        *e.ptr = value;
        return true;
      }
    }
    return false;
  }
};

#define TUNABLE(name, value, min, max, step)                                                       \
  inline int name = value;                                                                         \
  inline struct name##_Register {                                                                  \
    name##_Register() { TunableRegistry::instance().add(#name, &name, min, max, step); }           \
  } name##_instance;
#else
#define TUNABLE(name, value, min, max, step) constexpr int name = value;
#endif

namespace Lyra {

constexpr std::string_view NAME    = "Lyra";
constexpr std::string_view AUTHOR  = "Evan Fung";
constexpr std::string_view VERSION = "1.0";

constexpr size_t TT_SIZE       = 32;
constexpr size_t THREADS       = 1;
constexpr int    MOVE_OVERHEAD = 50;
constexpr U64    CLOCK_FREQ    = 2048;

// General constants
constexpr Depth  MaxDepth  = 256;
constexpr Ply    MaxPly    = 2048;
constexpr size_t MaxMoves  = 256;
constexpr Ply    Rule50Ply = 100;

// History Constant
constexpr Eval   HistMax      = 16384;
constexpr Eval   ContSize     = 2;
constexpr size_t CorrHistSize = 32768;
constexpr Eval   CorrHistMax  = 1024;

// General search constants
constexpr Depth DepthQS     = 0;
constexpr Depth StackOffset = ContSize;

// SEE values
constexpr Eval SeePieceVals[NPieceType] = {170, 440, 460, 710, 1320, 0};

// Eval constants
constexpr Eval EvalInf       = 30000;
constexpr Eval EvalInvalid   = -EvalInf - 1;
constexpr Eval EvalMate      = 29000;
constexpr Eval EvalMateBound = EvalMate - MaxDepth;
constexpr Eval EvalDraw      = 0;
constexpr Eval EvalStop      = 0;

// Search constants
constexpr Eval RFPMargin = 100;

TUNABLE(LmrBaseQuiet, 768, 250, 2000, 100);
TUNABLE(LmrMultQuiet, 2560, 1500, 4000, 400);
TUNABLE(LmrBaseCap, 360, 250, 2000, 100);
TUNABLE(LmrMultCap, 3072, 1500, 4000, 400);
TUNABLE(LmrMultHist, 9000, 5000, 15000, 500);

} // namespace Lyra
