#pragma once

#include "board.hpp"
#include "defs.hpp"
#include "utils.hpp"

namespace Lyra {

using Killer   = NDArray<Move, 2>;
using MainHist = NDArray<Eval, NPiece, NSquare>;

struct PieceFromTo {
  Piece  pc;
  Square from;
  Square to;
};

PieceFromTo piece_from_to(const Board &board, Move move);
void        update_killer(Killer &killer, Move best);
void        update_hist(Eval &hist, Eval bonus);

} // namespace Lyra
