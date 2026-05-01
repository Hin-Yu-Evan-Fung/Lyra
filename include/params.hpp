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

TUNABLE(LmrBaseQuiet, 768, 250, 2000, 100);
TUNABLE(LmrMultQuiet, 4096, 1024, 6144, 400);
TUNABLE(LmrMultHist, 7000, 4000, 12000, 500);

}; // namespace Lyra
