#pragma once

#include "utils.hpp"

#include <atomic>

namespace Lyra {

enum class TCType {
  Variable,
  Fixed,
  Nodes,
  Depth,
  Infinite,
};

struct TimeControl {
  Time  time[NColour];
  Time  inc[NColour];
  Depth depth;
  U16   moves_to_go;
  U64   nodes;
  Time  move_time;
  bool  is_infinite;
};

class Clock {
public:
  Clock(std::atomic_bool &stop)
      : stop_(stop)
      , type_(TCType::Infinite) {}

  void set(Colour stm, TimeControl tc);
  Time elapsed() const { return now() - start_; }

  bool stop_iter(Depth depth, Depth last_best_move_depth, Eval avg_eval, Eval eval, U64 nodes,
                 Move best_move);
  bool stop(U64 nodes);
  void update_effort(U64 nodes, Move move);

private:
  void calc_time();

  std::atomic_bool &stop_;

  NDArray<U64, NSquare, NSquare> effort_;

  TimeControl tc_;
  TCType      type_;
  Time        start_;
  Time        opt_;
  Time        max_;

  int pv_stability_;

  std::atomic_uint64_t total_nodes;
};

} // namespace Lyra
