#pragma once

#include <algorithm>
#include <cstdlib>

#include "board/board.hpp"
#include "core/defs.hpp"
#include "utils/utils.hpp"

namespace Lyra {

struct Killer {
  NDArray<Move, 2> moves;

  void clear() { moves.fill(NoMove); }

  void update(Move best) {
    if (best == moves[0])
      return;
    moves[1] = moves[0];
    moves[0] = best;
  }
};

struct History {
  Eval eval;

  void update(Eval bonus) {
    int clamped = std::clamp(bonus, -HistoryMax, HistoryMax);
    eval += clamped - eval * std::abs(clamped) / HistoryMax;
  }
};

struct MainHistory {
  void clear() { history.fill({}); }

  Eval get(const Board &board, Move move) const {
    using namespace MoveUtils;
    return history[board.on(src(move))][dst(move)].eval;
  }

  void update(const Board &board, Move move, Eval bonus) {
    using namespace MoveUtils;
    history[board.on(src(move))][dst(move)].update(bonus);
  }

private:
  NDArray<History, NPiece, NSquare> history;
};

} // namespace Lyra
