#pragma once

#include "board.hpp"
#include "defs.hpp"
#include "params.hpp"
#include "utils.hpp"

namespace Lyra {

using Killer    = NDArray<Move, 2>;
using MainHist  = NDArray<Eval, NPiece, NSquare>;
using ContHist  = NDArray<Eval, NPiece, NSquare>;
using ContTable = NDArray<ContHist, NPiece, NSquare>;
using CapHist   = NDArray<Eval, NPiece, NSquare, NPieceType>;
using CorrHist  = NDArray<Eval, NColour, CorrHistSize>;

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

template <Eval Max>
constexpr void hist_gravity(Eval &hist, Eval bonus) {
  hist += bonus - hist * std::abs(bonus) / Max;
}

constexpr void update_hist_quiet(MainHist &hist, const Board &board, Move move, Eval bonus) {
  const PieceTo p = piece_to(board, move);
  hist_gravity<HistMax>(hist[p.pc][p.to], bonus);
}

constexpr void update_hist_cap(CapHist &hist, const Board &board, Move move, Eval bonus) {
  const PieceTo   p   = piece_to(board, move);
  const PieceType vic = board.captured(move);
  hist_gravity<HistMax>(hist[p.pc][p.to][vic], bonus);
}

constexpr void update_hist_cont(ContHist &hist, const Board &board, Move move, Eval bonus) {
  const PieceTo p = piece_to(board, move);
  hist_gravity<HistMax>(hist[p.pc][p.to], bonus);
}

constexpr void update_hist_corr(CorrHist &hist, const Board &board, Depth depth, Eval best,
                                Eval eval) {
  const Eval MaxDiff = CorrHistMax / 4;
  const Eval bonus   = std::clamp((best - eval) * depth / 8, -MaxDiff, MaxDiff);
  hist_gravity<CorrHistMax>(hist[board.stm()][board.pawn_key() % CorrHistSize], bonus);
}

} // namespace Lyra
