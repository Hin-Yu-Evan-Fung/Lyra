#include "wrapper.hpp"

#include "board.hpp"
#include "clock.hpp"
#include "network.hpp"

#include <fstream>

namespace Lyra {

std::atomic<bool> engine_stop{false};

void initialise() {
  BBUtils::init();
  Zobrist::init();
  std::ifstream        f(NETWORK_PATH, std::ios::binary);
  std::vector<uint8_t> data{std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
  NNUE::load_network(data.data(), data.size());
}

uintptr_t get_stop_flag_address() { return reinterpret_cast<uintptr_t>(&engine_stop); }

// clang-format off
LyraWasm::LyraWasm()
    : tt_(DefaultTTSize)
    , worker_(engine_stop, 0, tt_) {
  worker_.register_callbacks({
    .on_best_move = [this](Move m) {
          if (!on_best_move_.isUndefined())
            on_best_move_(MoveUtils::format(m, false));
        },
    .on_depth_finished = [this](PrintInfo info) {
          if (!on_depth_finished_.isUndefined())
            on_depth_finished_(
                info.depth, info.seldepth, info.eval,
                info.time, info.nodes, info.nps,
                info.hashfull, info.pv
            );
        }
  });
}
// clang-format on

void LyraWasm::on_best_move(emscripten::val cb) { on_best_move_ = cb; }
void LyraWasm::on_depth_finished(emscripten::val cb) { on_depth_finished_ = cb; }

void LyraWasm::set_position(const std::string &fen) { board_.set(fen); }

void LyraWasm::go_move_time(int movetime) {
  TimeControl tc{};
  tc.move_time = movetime;
  engine_stop.store(false, std::memory_order::relaxed);
  worker_.reset(board_);
  worker_.start(tc);
}

void LyraWasm::go_depth(int depth) {
  TimeControl tc{};
  tc.depth = depth;
  engine_stop.store(false, std::memory_order::relaxed);
  worker_.reset(board_);
  worker_.start(tc);
}

void LyraWasm::go_time(int wtime, int btime, int winc, int binc) {
  TimeControl tc{};
  tc.time[White] = wtime;
  tc.time[Black] = btime;
  tc.inc[White]  = winc;
  tc.inc[Black]  = binc;
  engine_stop.store(false, std::memory_order::relaxed);
  worker_.reset(board_);
  worker_.start(tc);
}

void LyraWasm::go_infinite() {
  TimeControl tc{};
  tc.is_infinite = true;
  engine_stop.store(false, std::memory_order::relaxed);
  worker_.reset(board_);
  worker_.start(tc);
}

} // namespace Lyra
