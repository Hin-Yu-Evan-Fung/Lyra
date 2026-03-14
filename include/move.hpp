#pragma once
#include "defs.hpp"
#include "utils.hpp"

#include <string>

namespace Lyra {

/******************************************\
|==========================================|
|            Move representation           |
| Bits (0-5): src square                   |
| Bits (6-11): dst square                  |
| Bits (12-15): flags                      |
|==========================================|
\******************************************/

class Board;

// Move flag enum
enum MoveFlag : U16 {
  Quiet       = 0b0000 << 12,
  DoublePush  = 0b0001 << 12,
  KingCastle  = 0b0010 << 12,
  QueenCastle = 0b0011 << 12,
  Cap         = 0b1000 << 12,
  EP          = 0b1001 << 12,
  Promo_N     = 0b0100 << 12,
  Promo_B     = 0b0101 << 12,
  Promo_R     = 0b0110 << 12,
  Promo_Q     = 0b0111 << 12,
  PromoCap_N  = 0b1100 << 12,
  PromoCap_B  = 0b1101 << 12,
  PromoCap_R  = 0b1110 << 12,
  PromoCap_Q  = 0b1111 << 12,
};

constexpr Move NoMove   = 0ULL;
constexpr Move NullMove = 0ULL;

/******************************************\
|==========================================|
|              Move Utilities              |
|==========================================|
\******************************************/

namespace MoveUtils {

constexpr U16 FlagMask       = 0xF << 12;
constexpr U16 SrcMask        = 0x3F;
constexpr U16 DstMask        = SrcMask << 6;
constexpr U16 CapMask        = 0b1000 << 12;
constexpr U16 PromoMask      = 0b0100 << 12;
constexpr U16 CastleMask     = 0b0010 << 12;
constexpr U16 PromoPieceMask = 0b0011 << 12;

template <MoveFlag Flag>
constexpr Move encode(Square src, Square dst) {
  return Move(src | dst << 6 | Flag);
}

constexpr Square   src(Move move) { return static_cast<Square>(move & SrcMask); }
constexpr Square   dst(Move move) { return static_cast<Square>((move & DstMask) >> 6); }
constexpr MoveFlag flag(Move move) { return static_cast<MoveFlag>(move & FlagMask); }

constexpr bool is_ep(Move move) { return flag(move) == EP; }
constexpr bool is_capture(Move move) { return flag(move) & CapMask; }
constexpr bool is_promo(Move move) { return flag(move) & PromoMask; }
constexpr bool is_castle(Move move) {
  return flag(move) == KingCastle || flag(move) == QueenCastle;
}

constexpr PieceType promoted_pt(Move move) {
  return static_cast<PieceType>(((flag(move) & PromoPieceMask) >> 12) + N);
}

constexpr std::string format(Move move, bool chess960) {
  if (!move) return "none";

  Square src_sq = src(move);
  Square dst_sq = dst(move);

  if (!chess960 && is_castle(move))
    dst_sq = make_square(flag(move) == QueenCastle ? FileC : FileG, rank_of(src_sq));

  std::string move_str = IOUtils::format_sq(src_sq) + IOUtils::format_sq(dst_sq);

  if (is_promo(move)) move_str += "pnbrqk "[promoted_pt(move)];

  return move_str;
}

} // namespace MoveUtils

} // namespace Lyra
