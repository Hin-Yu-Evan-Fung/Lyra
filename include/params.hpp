#pragma once

#include "defs.hpp"

#include <string_view>

#ifdef TUNE

/******************************************\
|==========================================|
|                 Tunables                 |
|==========================================|
\******************************************/

// Provides macros and structs to manage tunable values in a SPSA tune.

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

  // Sets the tunable value based on name
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

/******************************************\
|==========================================|
|             Engine Paramters             |
|==========================================|
\******************************************/

namespace Lyra {

static constexpr std::string_view EngineName    = "Lyra";
static constexpr std::string_view EngineAuthor  = "Evan Fung";
static constexpr std::string_view EngineVersion = "1.0";

static constexpr size_t DefaultTTSize  = 32;
static constexpr size_t DefaultThreads = 1;
static constexpr int    MoveOverhead   = 50;
static constexpr U64    ClockFrequency = 2048;

// General constants
static constexpr Depth  MaxDepth  = 256;
static constexpr Ply    MaxPly    = 2048;
static constexpr size_t MaxMoves  = 256;
static constexpr Ply    Rule50Ply = 100;

// Hash table constants
static constexpr size_t BucketSize = 3;
static constexpr U64    AgeMask    = 0x3FUL;
static constexpr int    BoundShift = 6;
static constexpr U64    BoundMask  = 0x3UL << BoundShift;
static constexpr Depth  TTDepthOff = -3; // Buffer for TT depth flags

// History Constant
static constexpr Eval   HistMax      = 16384;
static constexpr Eval   ContSize     = 2;
static constexpr size_t CorrHistSize = 32768;
static constexpr Eval   CorrHistMax  = 1024;

// SEE values
static constexpr Eval SeePieceVals[NPieceType] = {170, 440, 460, 710, 1320, 0};

// Eval constants
static constexpr Eval EvalInf       = 30000;
static constexpr Eval EvalInvalid   = -EvalInf - 1;
static constexpr Eval EvalMate      = 29000;
static constexpr Eval EvalMateBound = EvalMate - MaxDepth;
static constexpr Eval EvalDraw      = 0;
static constexpr Eval EvalStop      = 0;

// Search constants
static constexpr Depth DepthQS        = 0;
static constexpr Depth StackOffset    = ContSize;
static constexpr Depth NMPDepth       = 2;
static constexpr Depth NmpBase        = 3;
static constexpr Depth NmpMult        = 5;
static constexpr Depth SEEPruneDepth  = 10;
static constexpr Eval  SEENoisyMargin = 20;
static constexpr Eval  SEEQuietMargin = 70;
static constexpr Depth HPDepth        = 2;
static constexpr Eval  HPMult         = 2000;
static constexpr Depth LMPDepth       = 8;
static constexpr Depth LMPBase        = 3;
static constexpr Depth LMPMult        = 2;
static constexpr Depth RFPDepth       = 8;
static constexpr Eval  RFPMult        = 100;
static constexpr Depth FPDepth        = 5;
static constexpr Eval  FPBase         = 100;
static constexpr Eval  FPMult         = 100;
static constexpr Depth LMRDepth       = 3;
static constexpr int   LMRMoveCount   = 3;
static constexpr Depth SingularDepth  = 8;
static constexpr Depth SingularMult   = 2;
static constexpr Eval  QFPMargin      = 300;
static constexpr Eval  QSEEMargin     = 30;

TUNABLE(LmrBaseQuiet, 768, 250, 2000, 100);
TUNABLE(LmrMultQuiet, 4096, 1024, 6144, 400);
TUNABLE(LmrMultHist, 7000, 4000, 12000, 500);

}; // namespace Lyra
