#include <algorithm>
#include <cstdlib>

#include "board.hpp"
#include "defs.hpp"
#include "utils.hpp"

namespace Lyra {

struct Killer {
  NDArray<Move, 2> moves;

  void clear() { moves.fill(NoMove); }

  void update(Move best) {
    if (best != moves[0]) {
      moves[1] = moves[0];
      moves[0] = best;
    }
  }
};

template <Eval Max>
struct History {
  Eval eval;

  void update(Eval bonus) {
    int clamped  = std::clamp(bonus, -Max, Max);
    eval        += clamped - eval * std::abs(clamped) / Max;
  }
};

struct MainHistory {
  NDArray<History<HistoryMax>, NPiece, NSquare> history;

  void clear() { history.fill({}); }

  History<HistoryMax>& get(const Board& board, Move move) {
    Piece  moved = board.on(MoveUtils::src(move));
    Square to    = MoveUtils::dst(move);

    return history[moved][to];
  }
};

}  // namespace Lyra
