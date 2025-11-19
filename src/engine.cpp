#include "engine.hpp"

#include <atomic>

#include "defs.hpp"
#include "movegen.hpp"
#include "perft.hpp"

namespace Lyra {

Engine::Engine() : pool_(THREADS) {}

void Engine::set_pos(const std::string fen, const std::vector<std::string>& moves) {
  if (is_busy()) return;
  board_.set(fen);

  for (std::string move_str : moves) {
    Move parsed = NoMove;

    for (Move move : list_moves(board_)) {
      if (to_str(move) == move_str) {
        parsed = move;
        break;
      }
    }

    if (parsed != NoMove)
      board_.do_move(parsed);
    else
      throw std::invalid_argument(std::format("Move %s is illegal!", move_str.data()));
  }
}

void Engine::newgame() {
  if (!is_busy()) { board_.set(start_pos.data()); }
}

void Engine::set_threads(size_t num) {
  if (!is_busy()) pool_.resize(num);
}

void Engine::go(const TimeControl& tc) {
  if (is_busy()) return;

  pool_.stop_.store(false, std::memory_order::relaxed);
  pool_.exec([&](Thread& th) {
    th.worker_.reset(board_.fen());
    th.worker_.start(tc);
  });
}

template <PerftMode PM>
void Engine::perft(Depth d) {
  if (!is_busy()) Lyra::perft<PM>(board_, d);
}

void Engine::perft_bench() {
  if (!is_busy()) Lyra::perft_bench();
}

template void Engine::perft<Perft>(Depth d);
template void Engine::perft<Perft_MP>(Depth d);

}  // namespace Lyra
