#pragma once

#include "board/board.hpp"
#include "core/defs.hpp"
#include "utils/utils.hpp"

#include <algorithm>
#include <cstdlib>

namespace Lyra {

using namespace MoveUtils;

/******************************************\
|==========================================|
|               Killer Moves               |
|==========================================|
\******************************************/

struct Killer {
  NDArray<Move, 2> moves;

  void clear() { moves.fill(NoMove); }

  void update(Move best) {
    if (best == moves[0]) return;
    moves[1] = moves[0];
    moves[0] = best;
  }
};

/******************************************\
|==========================================|
|              History Entries             |
|==========================================|
\******************************************/

template <Eval Max> struct Hist {
  Eval eval;

  void update(Eval bonus) {
    int clamped = std::clamp(bonus, -Max, Max);
    eval += clamped - eval * std::abs(clamped) / Max;
  }
};

/******************************************\
|==========================================|
|            History Heuristics            |
|==========================================|
\******************************************/

struct MainHist {
  void clear() { history.fill({}); }

  Eval get(const Board &board, Move move) const {
    const Piece attacker = board.on(src(move));
    return history[attacker][dst(move)].eval;
  }
  void update(const Board &board, Move move, Eval bonus) {
    const Piece attacker = board.on(src(move));
    history[attacker][dst(move)].update(bonus);
  }

private:
  NDArray<Hist<HistMax>, NPiece, NSquare> history;
};

struct CapHist {
  void clear() { history.fill({}); }

  Eval get(const Board &board, Move move) const {
    const Piece attacker = board.on(src(move));
    const PieceType victim = is_ep(move) ? P : board.pt_on(dst(move));
    return history[attacker][dst(move)][victim].eval;
  }

  void update(const Board &board, Move move, Eval bonus) {
    const Piece attacker = board.on(src(move));
    const PieceType victim = is_ep(move) ? P : board.pt_on(dst(move));
    history[attacker][dst(move)][victim].update(bonus);
  }

private:
  NDArray<Hist<HistMax>, NPiece, NSquare, NPieceType> history;
};

} // namespace Lyra
