#include "engine.hpp"

#include <atomic>
#include <iostream>

#include "defs.hpp"
#include "movegen.hpp"
#include "perft.hpp"

namespace Lyra {

Engine::Engine() : pool_(THREADS) {}

/******************************************\
|==========================================|
|                 Methods                  |
|==========================================|
\******************************************/

void Engine::newgame() {
  if (!is_busy()) { board_.set(start_pos.data()); }
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

/******************************************\
|==========================================|
|                 Setters                  |
|==========================================|
\******************************************/

void Engine::set_pos(const std::string fen, const std::vector<std::string>& moves) {
  if (is_busy()) return;
  board_.set(fen);

  for (std::string move_str : moves) {
    Move parsed = NoMove;

    for (Move move : list_moves(board_)) {
      std::string move_repr = MoveUtils::format(move, board_.chess960);
      MoveFlag    flag      = MoveUtils::flag(move);

      if (move_repr == move_str || (flag == KingCastle && move_str == "O-O") ||
          (flag == QueenCastle && move_str == "O-O-O")) {
        parsed = move;
        break;
      }
    }

    if (parsed != NoMove)
      board_.do_move(parsed);
    else
      throw std::invalid_argument(std::format("Move {} is illegal!", move_str));
  }
}

void Engine::set_threads(size_t num) {
  if (!is_busy()) pool_.resize(num);
}

void Engine::set_chess960(bool chess960) { board_.chess960 = chess960; }

template void Engine::perft<Perft>(Depth d);
template void Engine::perft<Perft_MP>(Depth d);

}  // namespace Lyra
