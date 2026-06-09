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
using HistCorr  = NDArray<Eval, NColour, CorrHistSize>;

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

constexpr void update_hist_quiet(HistQuiet *hist, const Board &board, Move move, Eval bonus) {
  const PieceTo p = board.piece_to(move);
  hist_gravity<HistMax>((*hist)[p.pc][p.to], bonus);
}

constexpr void update_hist_corr(HistCorr *hist, const Board &board, Depth depth, Eval best,
                                Eval eval) {
  const Eval MaxDiff = CorrHistMax / 4;
  const Eval bonus   = std::clamp((best - eval) * depth / 8, -MaxDiff, MaxDiff);
  hist_gravity<CorrHistMax>((*hist)[board.stm()][board.pawn_key() % CorrHistSize], bonus);
}

} // namespace Lyra
