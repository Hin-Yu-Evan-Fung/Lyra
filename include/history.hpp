#pragma once

#include "board.hpp"
#include "defs.hpp"
#include "utils.hpp"

namespace Lyra {

using Killer    = NDArray<Move, 2>;
using MainHist  = NDArray<Eval, NPiece, NSquare>;
using ContHist  = NDArray<Eval, NPiece, NSquare>;
using ContTable = NDArray<ContHist, NPiece, NSquare>;
using CapHist   = NDArray<Eval, NPiece, NSquare, NPieceType>;

struct PieceTo {
  Piece  pc;
  Square to;
};

PieceTo piece_to(const Board &board, Move move);
void    update_killer(Killer &killer, Move best);
void    update_hist(Eval &hist, Eval bonus);

} // namespace Lyra
