#pragma once

#include "utils.hpp"

#include <atomic>

namespace Lyra {

struct TimeControl {
  Time  time[NColour];
  Time  inc[NColour];
  Depth depth;
  U16   moves_to_go;
  Time  move_time;
  bool  is_infinite;
};

class Clock {
public:
  Clock(std::atomic_bool &stop)
      : stop_(stop) {}

  void set(Colour stm, const TimeControl &tc);
  Time elapsed() const { return now() - start_; }

  bool stop_iter(Depth depth);
  bool stop(U64 nodes);

private:
  void calc_time();

  std::atomic_bool &stop_;

  Time start_;
  Time opt_;
  Time max_;

  Depth max_depth_;

  std::atomic_uint64_t total_nodes;
};

} // namespace Lyra
