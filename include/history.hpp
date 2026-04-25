#pragma once

#include "board.hpp"
#include "defs.hpp"
#include "params.hpp"
#include "utils.hpp"

namespace Lyra {

using Killer    = NDArray<Move, 2>;
using HistQuiet = NDArray<Eval, NPiece, NSquare>;
using HistCont  = NDArray<HistQuiet, NPiece, NSquare>;
using ContBuf   = NDArray<HistQuiet *, ContSize>;

struct PieceTo {
  Piece  pc;
  Square to;
};

constexpr PieceTo piece_to(const Board &board, Move move) {
  return {board.on(MoveUtils::src(move)), MoveUtils::dst(move)};
}

constexpr void update_killer(Killer &killer, Move best) {
  if (killer[0] != best) {
    killer[1] = killer[0];
    killer[0] = best;
  }
}

constexpr void hist_gravity(Eval &hist, Eval bonus) {
  hist += bonus - hist * std::abs(bonus) / HistMax;
}

constexpr void update_hist_quiet(HistQuiet &hist, const Board &board, Move move, Eval bonus) {
  const PieceTo p = piece_to(board, move);
  hist_gravity(hist[p.pc][p.to], bonus);
}

} // namespace Lyra
