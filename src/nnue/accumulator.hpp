#pragma once

#include "bitboard.hpp"
#include "board.hpp"
#include "defs.hpp"
#include "network.hpp"
#include "nnue_params.hpp"
#include "nnue_utils.hpp"
#include "simd.hpp"

namespace Lyra::NNUE {

/******************************************\
|==========================================|
|              NNUE Accumulator            |
|==========================================|
\******************************************/

struct Accumulator {
  SideAccumulator white;
  SideAccumulator black;

  NDArray<BB, NPieceType> pieces;
  NDArray<BB, NColour>    colour;

  Accumulator()
      : white{}
      , black{}
      , pieces{}
      , colour{} {}

  void update(const Board &board) {
    Square wksq = board.ksq<White>();
    Square bksq = board.ksq<Black>();

    for (Colour c : {White, Black}) {
      BB old_c = colour[c];
      BB new_c = board.bb(c);

      for (PieceType pt : {P, N, B, R, Q, K}) {
        BB old_pc = old_c & pieces[pt];
        BB new_pc = new_c & board.bb(pt);

        // Update new pieces
        bitloop(new_pc & ~old_pc, [&](Square sq) { update_weights<true>(c, pt, sq, wksq, bksq); });
        // Update old pieces
        bitloop(~new_pc & old_pc, [&](Square sq) { update_weights<false>(c, pt, sq, wksq, bksq); });
      }
    }

    colour = board.colour_bbs();
    pieces = board.piece_bbs();
  }

  I32 propagate(Colour c, size_t output_bucket) {
    const SideAccumulator &stm = c == White ? white : black;
    const SideAccumulator &opp = c == White ? black : white;

    I32 stmsum = dotprod(stm, network.out_weights[output_bucket][0]);
    I32 oppsum = dotprod(opp, network.out_weights[output_bucket][1]);

    return ((stmsum + oppsum) / QA + (I32)network.out_bias[output_bucket]) * SCALE / QAB;
  }

private:
  template <bool Add>
  static void update_side(SideAccumulator &acc, size_t idx) {
    for (unsigned i = 0; i < L1; ++i) {
      if constexpr (Add) {
        acc[i] += network.ft_weights[idx + i];
      } else {
        acc[i] -= network.ft_weights[idx + i];
      }
    }
  }

  template <bool Add>
  void update_weights(Colour c, PieceType pt, Square sq, Square wksq, Square bksq) {
    update_side<Add>(white, feature_index<White>(c, pt, sq, wksq) * L1);
    update_side<Add>(black, feature_index<Black>(c, pt, sq, bksq) * L1);
  }
};

} // namespace Lyra::NNUE
