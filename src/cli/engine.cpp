#include "engine.hpp"

#include "defs.hpp"
#include "movegen.hpp"
#include "params.hpp"
#include "perft.hpp"

#include <atomic>
#include <cstdio>

namespace Lyra {

Engine::Engine()
    : tt_(DefaultTTSize)
    , pool_(DefaultThreads, tt_) {

  callbacks_ = {
      .on_best_move      = [this](Move m) { on_best_move(m); },
      .on_depth_finished = [this](PrintInfo info) { on_depth_finished(info); },
  };

  pool_.register_callbacks(callbacks_);
}

/******************************************\
|==========================================|
|                 Callbacks                |
|==========================================|
\******************************************/

void Engine::on_best_move(Move m) const {
  std::println("bestmove {}", MoveUtils::format(m, false));
  std::fflush(stdout);
}

void Engine::on_depth_finished(PrintInfo info) const {
  std::println("info depth {} seldepth {} score {} time {} nodes {} nps {} hashfull {} pv {}",
               info.depth, info.seldepth, format_eval(info.eval), info.time, info.nodes, info.nps,
               info.hashfull, info.pv);
  std::fflush(stdout);
}

/******************************************\
|==========================================|
|                 Methods                  |
|==========================================|
\******************************************/

void Engine::newgame() {
  pool_.wait();
  board_.set(start_pos.data());
}

void Engine::go(const TimeControl &tc) {
  pool_.wait();
  pool_.stop_.store(false, std::memory_order::relaxed);
  pool_.exec([this, tc](Thread &th) {
    th.worker_.reset(board_);
    th.worker_.start(tc);
  });
}

void Engine::perft(PerftMode pm, Depth d) {
  pool_.wait();
  Lyra::perft(pm, board_, d);
}

void Engine::perft_bench() {
  pool_.wait();
  Lyra::perft_bench();
}

/******************************************\
|==========================================|
|                 Setters                  |
|==========================================|
\******************************************/

void Engine::set_pos(const std::string fen, const std::vector<std::string> &moves) {
  pool_.wait();
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
  pool_.wait();
  pool_.resize(num, tt_);
  pool_.register_callbacks(callbacks_);
}

void Engine::set_tt_size(size_t mb) {
  pool_.wait();
  tt_.resize(mb);
}

void Engine::clear_tt() {
  pool_.wait();
  tt_.clear();
}

void Engine::set_chess960(bool chess960) { board_.chess960 = chess960; }

} // namespace Lyra
