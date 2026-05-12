#include "clock.hpp"

#include "defs.hpp"
#include "move.hpp"

#include <atomic>

namespace Lyra {

using namespace MoveUtils;

void Clock::set(Colour stm, TimeControl tc) {
  tc_ = tc;

  if (tc_.is_infinite)
    type_ = TCType::Infinite;
  else if (tc_.depth)
    type_ = TCType::Depth;
  else if (tc_.move_time)
    type_ = TCType::Fixed;
  else {
    type_ = TCType::Variable;

    const Time time = tc.time[stm];
    const Time inc  = tc.inc[stm];
    const int  mtg  = tc.moves_to_go;

    if (mtg > 0) {
      const double scale  = 0.7 / std::min(mtg, 50);
      const double eighth = 0.8 * time;

      opt_ = std::min(scale * time, eighth);
      max_ = std::min(5.0 * opt_, eighth);
    } else {
      const double total = ((double)time / 20) + ((double)inc * 3 / 4);
      opt_               = total * 0.6;
      max_               = std::min(2.0 * total, (double)time);
    }

    opt_ = std::min(opt_, time - MoveOverhead);
    max_ = std::min(max_, time - MoveOverhead);
  }

  pv_stability_ = 0;
  effort_       = {};
  start_        = now();
  total_nodes.store(0, std::memory_order_relaxed);
}

bool Clock::stop_iter(Depth depth, Depth last_best_move_depth, Eval avg_eval, Eval eval, U64 nodes,
                      Move best_move) {
  if (stop_.load(std::memory_order::relaxed)) return true;
  if (depth < 4) return false;

  bool stop = false;

  switch (type_) {
  case TCType::Infinite: return false;
  case TCType::Depth: stop = depth >= tc_.depth; break;
  case TCType::Fixed: stop = elapsed() > tc_.move_time; break;
  case TCType::Nodes: stop = nodes >= tc_.nodes; break;
  case TCType::Variable: {

    pv_stability_ = last_best_move_depth + 3 <= depth ? std::min(10, pv_stability_ + 1) : 0;

    // Use a combination of pv stability, score fluctuations, and effort to determine how much more
    // time we want to spend on this move.

    const double pv_factor = 1.2 - 0.04 * pv_stability_;

    const double score_flucuations = std::abs(avg_eval - eval);
    const double score_factor      = std::clamp(0.05 * score_flucuations, 0.75, 1.25);

    const U64    effort         = effort_[src(best_move)][dst(best_move)];
    const double effort_percent = 1.0 - (double)effort / (double)nodes;
    const double effort_factor  = std::max(0.5, 2 * effort_percent + 0.4);

    const Time total_time = opt_ * pv_factor * score_factor * effort_factor;

    stop = elapsed() > total_time;
  }
  }

  if (stop) stop_all_threads();
  return stop;
}

bool Clock::stop(U64 nodes) {
  U64 searched = nodes - total_nodes.load(std::memory_order::relaxed);

  if (searched >= ClockFrequency) {
    total_nodes.fetch_add(searched);
    if (stop_.load(std::memory_order::relaxed)) return true;
  }

  bool stop = false;

  // Prevent polling too frequently
  if (searched >= ClockFrequency) {
    switch (type_) {
    case TCType::Infinite: return false;
    case TCType::Fixed: stop = elapsed() > tc_.move_time; break;
    case TCType::Nodes: stop = nodes >= tc_.nodes; break;
    case TCType::Variable: stop = elapsed() > max_; break;
    default: stop = false;
    }
  }

  if (stop) stop_all_threads();
  return stop;
}

void Clock::update_effort(U64 nodes, Move move) { effort_[src(move)][dst(move)] += nodes; }

} // namespace Lyra
