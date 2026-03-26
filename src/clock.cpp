#include "clock.hpp"

#include "defs.hpp"
#include "move.hpp"

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
      opt_ = 1.80 * (time - MOVE_OVERHEAD) / mtg + inc;
      max_ = 10.00 * (time - MOVE_OVERHEAD) / mtg + inc;
    } else {
      opt_ = 2.50 * (time - MOVE_OVERHEAD + 25 * inc) / 50;
      max_ = 10.00 * (time - MOVE_OVERHEAD + 25 * inc) / 50;
    }

    opt_ = std::min(opt_, time - MOVE_OVERHEAD);
    max_ = std::min(max_, time - MOVE_OVERHEAD);
  }

  effort_ = {};
  start_  = now();
}

bool Clock::stop_iter(Depth depth, Depth last_best_move_depth, Eval avg_eval, Eval eval, U64 nodes,
                      Move best_move) {
  if (stop_.load(std::memory_order::relaxed)) return true;
  if (max_ == 0 && max_depth_ == 0) return false;

  bool stable     = last_best_move_depth + 3 <= depth;
  pv_stability_   = stable ? std::min(pv_stability_ + 1, 10) : 0;
  float pv_factor = 1.2 - 0.04f * (float)pv_stability_;

  Eval  score_fluctuations = std::abs(avg_eval - eval);
  float score_factor       = std::clamp(0.05 * score_fluctuations, 0.75, 1.25);

  U64   best_move_effort  = effort_[MoveUtils::src(best_move)][MoveUtils::dst(best_move)];
  float best_move_percent = 1.0 - (float)best_move_effort / (float)nodes;
  float best_move_factor  = std::max(2.0 * best_move_percent + 0.4, 0.5);

  bool stop = (opt_ > 0 && elapsed() >= opt_ * best_move_factor * score_factor * pv_factor)
              || (max_depth_ > 0 && depth >= max_depth_);
  if (stop) stop_.store(true, std::memory_order::relaxed);
  return stop;
}

bool Clock::stop(U64 nodes) {
  U64 searched = nodes - total_nodes.load(std::memory_order::relaxed);

  if (searched >= CLOCK_FREQ) {
    total_nodes.fetch_add(searched);
    if (stop_.load(std::memory_order::relaxed)) return true;
  }

  if (max_ == 0) return false;

  bool stop = searched >= CLOCK_FREQ && elapsed() >= max_;
  if (stop) std::println("Stopping!");
  if (stop) stop_.store(true, std::memory_order::relaxed);
  return stop;
}

void Clock::update_effort(U64 nodes, Move move) {
  effort_[MoveUtils::src(move)][MoveUtils::dst(move)] += nodes;
}

} // namespace Lyra
