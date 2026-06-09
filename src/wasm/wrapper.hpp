#include "params.hpp"
#include "search.hpp"
#include "tt.hpp"

#include <atomic>
#include <emscripten/bind.h>

namespace Lyra {

extern std::atomic<bool> engine_stop; // your global stop atomic

void      initialise();
uintptr_t get_stop_flag_address();

class LyraWasm {
  Board           board_;
  TT              tt_;
  Worker          worker_;
  emscripten::val on_best_move_      = emscripten::val::undefined();
  emscripten::val on_depth_finished_ = emscripten::val::undefined();

public:
  LyraWasm();

  void on_best_move(emscripten::val cb);
  void on_depth_finished(emscripten::val cb);
  void set_position(const std::string &fen);
  void go_move_time(int movetime);
  void go_depth(int depth);
  void go_time(int wtime, int btime, int winc, int binc);
  void go_infinite();
};

EMSCRIPTEN_BINDINGS(Lyra) {
  emscripten::function("initialise", &initialise);
  emscripten::function("get_stop_flag_address", &get_stop_flag_address);

  emscripten::class_<LyraWasm>("LyraWasm")
      .constructor()
      .function("onBestMove", &LyraWasm::on_best_move)
      .function("onDepthFinished", &LyraWasm::on_depth_finished)
      .function("setPosition", &LyraWasm::set_position)
      .function("goMoveTime", &LyraWasm::go_move_time)
      .function("goDepth", &LyraWasm::go_depth)
      .function("goTime", &LyraWasm::go_time)
      .function("goInfinite", &LyraWasm::go_infinite);
};

} // namespace Lyra
