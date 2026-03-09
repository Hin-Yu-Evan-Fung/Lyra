#include "engine/engine.hpp"

#include "board/movegen.hpp"
#include "core/defs.hpp"
#include "utils/perft.hpp"

#include <atomic>
#include <print>

namespace Lyra {

Engine::Engine()
    : pool_(NThreads, tt_)
    , tt_(TTSize) {}

/******************************************\
|==========================================|
|                 Methods                  |
|==========================================|
\******************************************/

void Engine::print_eval() {
  std::println("Incremental Eval: {}", board_.eval());
  std::println("Raw Eval: {}", board_.compute_raw_eval());
}

void Engine::newgame() {
  if (!is_busy()) {
    board_.set(start_pos.data());
  }
}

void Engine::go(const TimeControl &tc) {
  if (is_busy()) return;

  pool_.stop_.store(false, std::memory_order::relaxed);
  pool_.exec([tc, this](Thread &th) {
    th.worker_.reset(board_);
    th.worker_.start(tc);
  });
}

void Engine::perft(Depth d, PerftMode mode) {
  if (!is_busy()) Lyra::perft(board_, d, mode);
}

/******************************************\
|==========================================|
|                 Setters                  |
|==========================================|
\******************************************/

void Engine::set_pos(const std::string fen, const std::vector<std::string> &moves) {
  if (is_busy()) return;
  board_.set(fen);

  for (std::string move_str : moves) {
    Move parsed = NoMove;

    for (Move move : list_moves(board_)) {
      std::string move_repr = MoveUtils::format(move, board_.chess960);
      MoveFlag    flag      = MoveUtils::flag(move);

      if (move_repr == move_str || (flag == KingCastle && move_str == "O-O")
          || (flag == QueenCastle && move_str == "O-O-O")) {
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
  if (!is_busy()) pool_.resize(num, tt_);
}

void Engine::set_tt_size(size_t mb) {
  if (!is_busy()) tt_.resize(mb);
}

void Engine::clear_tt() {
  if (!is_busy()) tt_.clear();
}

void Engine::set_chess960(bool chess960) { board_.chess960 = chess960; }

} // namespace Lyra
