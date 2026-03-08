#include "engine/clock.hpp"

#include "core/defs.hpp"
#include "engine/engine.hpp"

#include <atomic>
#include <print>

namespace Lyra {

void Clock::set(Colour stm, const TimeControl &tc) {
  if (tc.is_infinite || tc.depth) {
    opt_ = max_ = 0;
    max_depth_  = tc.depth;
  } else if (tc.move_time)
    opt_ = max_ = tc.move_time;
  else {
    const Time time = tc.time[stm];
    const Time inc  = tc.inc[stm];
    const U16  mtg  = tc.moves_to_go;

    if (mtg > 0) {
      opt_ = 1.80 * (time - MoveOverhead) / mtg + inc;
      max_ = 10.00 * (time - MoveOverhead) / mtg + inc;
    } else {
      opt_ = 2.50 * (time - MoveOverhead + 25 * inc) / 50;
      max_ = 10.00 * (time - MoveOverhead + 25 * inc) / 50;
    }

    opt_ = std::min(opt_, time - MoveOverhead);
    max_ = std::min(max_, time - MoveOverhead);
  }

  start_ = now();
}

bool Clock::stop_iter(Depth depth) {
  if (stop_.load(std::memory_order::relaxed)) return true;
  if (max_ == 0 && max_depth_ == 0) return false;

  bool stop = (opt_ > 0 && elapsed() >= opt_) || (max_depth_ > 0 && depth >= max_depth_);
  if (stop) stop_.store(true, std::memory_order::relaxed);
  return stop;
}

bool Clock::stop(U64 nodes) {
  U64 searched = nodes - total_nodes.load(std::memory_order::relaxed);

  if (searched >= ClockFreq) {
    total_nodes.fetch_add(searched);
    if (stop_.load(std::memory_order::relaxed)) return true;
  }

  if (max_ == 0) return false;

  bool stop = searched >= ClockFreq && elapsed() >= max_;
  if (stop) std::println("Stopping!");
  if (stop) stop_.store(true, std::memory_order::relaxed);
  return stop;
}

} // namespace Lyra
