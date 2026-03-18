#include "history.hpp"

#include "utils.hpp"

namespace Lyra {

PieceTo piece_to(const Board &board, Move move) {
  return {board.on(MoveUtils::src(move)), MoveUtils::dst(move)};
}

void update_killer(Killer &killer, Move best) {
  if (killer[0] != best) {
    killer[1] = killer[0];
    killer[0] = best;
  }
}

void update_hist(Eval &hist, Eval bonus) {
  Eval eval = std::clamp(bonus, -HistMax, HistMax);
  hist += eval - hist * std::abs(eval) / HistMax;
}

} // namespace Lyra
