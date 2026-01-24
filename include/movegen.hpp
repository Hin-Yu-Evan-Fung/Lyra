#pragma once

#include "bitboard.hpp"
#include "board.hpp"
#include "defs.hpp"
#include "move.hpp"

namespace Lyra {

using namespace BBUtils;
using namespace MoveUtils;

enum MoveGenType { GenQuiet = 1, GenCap = 2, GenAll = GenQuiet | GenCap };

template <Colour Us, bool IsCap, typename Handler>
constexpr void enum_promotions(Square src, Square dst, Handler&& handler) {
  constexpr MoveFlag QueenFlag  = IsCap ? PromoCap_Q : Promo_Q;
  constexpr MoveFlag RookFlag   = IsCap ? PromoCap_R : Promo_R;
  constexpr MoveFlag BishopFlag = IsCap ? PromoCap_B : Promo_B;
  constexpr MoveFlag KnightFlag = IsCap ? PromoCap_N : Promo_N;

  handler(encode<QueenFlag>(src, dst));
  handler(encode<RookFlag>(src, dst));
  handler(encode<BishopFlag>(src, dst));
  handler(encode<KnightFlag>(src, dst));
}

template <Colour Us, MoveGenType Gt, typename Handler>
constexpr void enum_pawn(const Board& board, Handler&& handler) {
  using enum Direction;
  constexpr Colour    Them       = ~Us;
  constexpr BB        push_rank  = Us == White ? from(Rank2) : from(Rank7);
  constexpr BB        promo_rank = Us == White ? from(Rank7) : from(Rank2);
  constexpr Direction Up         = Us == White ? N : S;
  constexpr Direction Down       = Us == White ? S : N;
  constexpr Direction UpWest     = Us == White ? NW : SW;
  constexpr Direction UpEast     = Us == White ? NE : SE;
  constexpr Direction DownWest   = Us == White ? SW : NW;
  constexpr Direction DownEast   = Us == White ? SE : NE;

  const BB     empty             = ~board.bb();
  const BB     enemy             = board.bb(Them);
  const BB     pawns             = board.bb(Us, P);
  const BB     diag_pin          = board.state()->diag_pin;
  const BB     hv_pin            = board.state()->hv_pin;
  const BB     check_mask        = board.state()->check_mask;
  const Square ep                = board.state()->ep;
  const BB     ep_target         = shift<Down>(from(ep));

  if constexpr (Gt & GenCap) {
    BB can_cap      = pawns & ~hv_pin;
    BB can_cap_west = can_cap & (~diag_pin | shift<DownWest>(diag_pin));
    BB can_cap_east = can_cap & (~diag_pin | shift<DownEast>(diag_pin));

    if (ep != NoSquare) {
      BB can_ep_west = can_cap_west & shift<W>(ep_target & check_mask);
      if (can_ep_west) handler(encode<EP>(lsb(can_ep_west), ep));

      BB can_ep_east = can_cap_east & shift<E>(ep_target & check_mask);
      if (can_ep_east) handler(encode<EP>(lsb(can_ep_east), ep));
    }

    can_cap_west &= shift<DownWest>(enemy & check_mask);
    can_cap_east &= shift<DownEast>(enemy & check_mask);

    if ((can_cap_west | can_cap_east) & promo_rank) {
      bitloop(can_cap_west & promo_rank, [&](Square src) {
        enum_promotions<Us, true>(src, shift<UpWest>(src), handler);
      });
      bitloop(can_cap_east & promo_rank, [&](Square src) {
        enum_promotions<Us, true>(src, shift<UpEast>(src), handler);
      });
      bitloop(can_cap_west & ~promo_rank, [&](Square src) { handler(encode<Cap>(src, shift<UpWest>(src))); });
      bitloop(can_cap_east & ~promo_rank, [&](Square src) { handler(encode<Cap>(src, shift<UpEast>(src))); });
    } else {
      bitloop(can_cap_west, [&](Square src) { handler(encode<Cap>(src, shift<UpWest>(src))); });
      bitloop(can_cap_east, [&](Square src) { handler(encode<Cap>(src, shift<UpEast>(src))); });
    }
  }

  if constexpr (Gt & GenQuiet) {
    BB semi_pushable   = pawns & ~diag_pin & shift<Down>(empty) & (~hv_pin | shift<Down>(hv_pin));
    BB can_double_push = semi_pushable & push_rank & shift<Down>(shift<Down>(empty & check_mask));
    BB can_push        = semi_pushable & shift<Down>(check_mask);

    if (can_push & promo_rank) {
      bitloop(can_push & promo_rank, [&](Square src) { enum_promotions<Us, false>(src, shift<Up>(src), handler); });
      bitloop(can_push & ~promo_rank, [&](Square src) { handler(encode<Quiet>(src, shift<Up>(src))); });
    } else
      bitloop(can_push, [&](Square src) { handler(encode<Quiet>(src, shift<Up>(src))); });

    bitloop(can_double_push, [&](Square src) { handler(encode<DoublePush>(src, shift<Up>(shift<Up>(src)))); });
  }
}

template <Colour Us, MoveGenType Gt, typename Handler>
constexpr void enum_knights(const Board& board, Handler&& handler) {
  constexpr Colour Them = ~Us;

  const BB empty        = ~board.bb();
  const BB enemy        = board.bb(Them);
  const BB diag_pin     = board.state()->diag_pin;
  const BB hv_pin       = board.state()->hv_pin;
  const BB check_mask   = board.state()->check_mask;

  BB attacks;
  BB moveable = board.bb(Us, N) & ~(diag_pin | hv_pin);
  bitloop(moveable, [&](Square src) {
    attacks = KNIGHT_ATK[src] & check_mask;

    if constexpr (Gt & GenCap) bitloop(attacks & enemy, [&](Square dst) { handler(encode<Cap>(src, dst)); });
    if constexpr (Gt & GenQuiet) bitloop(attacks & empty, [&](Square dst) { handler(encode<Quiet>(src, dst)); });
  });
}

template <Colour Us, MoveGenType Gt, typename Handler>
constexpr void enum_diag_slider(const Board& board, Handler&& handler) {
  constexpr Colour Them = ~Us;

  const BB occ          = board.bb();
  const BB empty        = ~occ;
  const BB enemy        = board.bb(Them);
  const BB diag_pin     = board.state()->diag_pin;
  const BB hv_pin       = board.state()->hv_pin;
  const BB check_mask   = board.state()->check_mask;

  BB attacks;
  BB moveable = board.bb(Us, B, Q) & ~hv_pin;
  bitloop(moveable & diag_pin, [&](Square src) {
    attacks = BISHOP_ATK[src][occ] & check_mask & diag_pin;

    if constexpr (Gt & GenCap) bitloop(attacks & enemy, [&](Square dst) { handler(encode<Cap>(src, dst)); });
    if constexpr (Gt & GenQuiet) bitloop(attacks & empty, [&](Square dst) { handler(encode<Quiet>(src, dst)); });
  });
  bitloop(moveable & ~diag_pin, [&](Square src) {
    attacks = BISHOP_ATK[src][occ] & check_mask;

    if constexpr (Gt & GenCap) bitloop(attacks & enemy, [&](Square dst) { handler(encode<Cap>(src, dst)); });
    if constexpr (Gt & GenQuiet) bitloop(attacks & empty, [&](Square dst) { handler(encode<Quiet>(src, dst)); });
  });
}

template <Colour Us, MoveGenType Gt, typename Handler>
constexpr void enum_hv_slider(const Board& board, Handler&& handler) {
  constexpr Colour Them = ~Us;

  const BB occ          = board.bb();
  const BB empty        = ~occ;
  const BB enemy        = board.bb(Them);
  const BB diag_pin     = board.state()->diag_pin;
  const BB hv_pin       = board.state()->hv_pin;
  const BB check_mask   = board.state()->check_mask;

  BB attacks;
  BB moveable = board.bb(Us, R, Q) & ~diag_pin;
  bitloop(moveable & hv_pin, [&](Square src) {
    attacks = ROOK_ATK[src][occ] & check_mask & hv_pin;

    if constexpr (Gt & GenCap) bitloop(attacks & enemy, [&](Square dst) { handler(encode<Cap>(src, dst)); });
    if constexpr (Gt & GenQuiet) bitloop(attacks & empty, [&](Square dst) { handler(encode<Quiet>(src, dst)); });
  });
  bitloop(moveable & ~hv_pin, [&](Square src) {
    attacks = ROOK_ATK[src][occ] & check_mask;

    if constexpr (Gt & GenCap) bitloop(attacks & enemy, [&](Square dst) { handler(encode<Cap>(src, dst)); });
    if constexpr (Gt & GenQuiet) bitloop(attacks & empty, [&](Square dst) { handler(encode<Quiet>(src, dst)); });
  });
}

template <Colour Us, MoveGenType Gt, typename Handler>
constexpr void enum_king(const Board& board, Handler&& handler) {
  constexpr Colour Them = ~Us;

  const Square ksq      = board.ksq<Us>();
  const BB     attacked = board.state()->attacked;
  const BB     enemy    = board.bb(Them);
  const BB     empty    = ~board.bb();

  BB attacks            = KING_ATK[ksq] & ~attacked;

  if constexpr (Gt & GenCap) bitloop(attacks & enemy, [&](Square dst) { handler(encode<Cap>(ksq, dst)); });
  if constexpr (Gt & GenQuiet) bitloop(attacks & empty, [&](Square dst) { handler(encode<Quiet>(ksq, dst)); });
}

template <Colour Us, typename Handler>
constexpr void enum_castle(const Board& board, Handler&& handler) {
  constexpr Castle OO   = Us == White ? WhiteOO : BlackOO;
  constexpr Castle OOO  = Us == White ? WhiteOOO : BlackOOO;

  const Castle cr       = board.state()->c_rights;
  const Square ksq      = board.ksq<Us>();
  const Square queen_rs = board.castle_mask().rook_src<Us>(true);
  const Square king_rs  = board.castle_mask().rook_src<Us>(false);

  if (cr & OO && board.can_castle<Us, false>()) handler(encode<KingCastle>(ksq, king_rs));
  if (cr & OOO && board.can_castle<Us, true>()) handler(encode<QueenCastle>(ksq, queen_rs));
}

template <Colour Us, MoveGenType Gt, typename Handler>
constexpr void enum_moves(const Board& board, Handler&& handler) {
  const BB check_mask = board.state()->check_mask;

  if (!check_mask)
    enum_king<Us, Gt>(board, handler);
  else {
    enum_pawn<Us, Gt>(board, handler);
    enum_knights<Us, Gt>(board, handler);
    enum_diag_slider<Us, Gt>(board, handler);
    enum_hv_slider<Us, Gt>(board, handler);
    enum_king<Us, Gt>(board, handler);

    if constexpr (Gt & GenQuiet)
      if (check_mask == FullBB) enum_castle<Us>(board, handler);
  }
}

constexpr std::vector<Move> list_moves(const Board& board) {
  std::vector<Move> moves;

  auto append = [&](Move move) { moves.push_back(move); };

  board.stm() == White ? enum_moves<White, GenAll>(board, append) : enum_moves<Black, GenAll>(board, append);

  return moves;
}

}  // namespace Lyra
