#include "bitboard.hpp"
#include "board.hpp"
#include "defs.hpp"

namespace Lyra {

using namespace BBUtils;

template <Colour Us>
constexpr BB Board::threatened() {
  constexpr Colour Them = ~Us;
  const BB         occ  = bb() ^ bb(Us, K);

  BB threatened         = pawn_attack_bb<Them>(bb(Them, P)) | KING_ATK[ksq<Them>()];
  bitloop(bb(Them, N), [&](Square src) { threatened |= KNIGHT_ATK[src]; });
  bitloop(bb(Them, B, Q), [&](Square src) { threatened |= BISHOP_ATK[src][occ]; });
  bitloop(bb(Them, R, Q), [&](Square src) { threatened |= ROOK_ATK[src][occ]; });

  return threatened;
}

template <Colour Us>
constexpr BB Board::checkers() {
  constexpr Colour Them = ~Us;

  const BB     occ      = bb();
  const Square ksq      = Board::ksq<Us>();

  return (PAWN_ATK[Us][ksq] & bb(Them, P)) | (KNIGHT_ATK[ksq] & bb(Them, N)) | (BISHOP_ATK[ksq][occ] & bb(Them, B, Q)) |
         (ROOK_ATK[ksq][occ] & bb(Them, R, Q));
}

template <Colour Us, bool inCheck>
constexpr void Board::update_pin_and_check_masks() {
  using enum Direction;
  constexpr Colour Them  = ~Us;

  const BB     their_occ = bb(Them);
  const BB     our_occ   = bb(Us);
  const Square ksq       = Board::ksq<Us>();
  BB           diag_pin = EmptyBB, hv_pin = EmptyBB, check_mask = EmptyBB;
  BB           pin_mask;

  BB pinners = BISHOP_ATK[ksq][their_occ] & bb(Them, B, Q);
  bitloop(pinners, [&](Square atk) {
    pin_mask = BTWN_BB[ksq][atk] | from(atk);
    switch (popcount(pin_mask & our_occ)) {
    case 0:
      if constexpr (inCheck) check_mask |= pin_mask;
      break;
    case 1: diag_pin |= pin_mask;
    }
  });

  pinners = ROOK_ATK[ksq][their_occ] & bb(Them, R, Q);
  bitloop(pinners, [&](Square atk) {
    pin_mask = BTWN_BB[ksq][atk] | from(atk);
    switch (popcount(pin_mask & our_occ)) {
    case 0:
      if constexpr (inCheck) check_mask |= pin_mask;
      break;
    case 1: hv_pin |= pin_mask;
    }
  });

  undo_->diag_pin = diag_pin;
  undo_->hv_pin   = hv_pin;
  if constexpr (inCheck) undo_->check_mask |= check_mask;
}

template <Colour Us>
void Board::update_masks() {
  const Square ksq            = Board::ksq<Us>();
  const BB     enemy_or_empty = ~bb(Us) | bb(Us, R);
  BB           b              = checkers<Us>();

  if (!b) {  // No checks
    undo_->check_mask = FullBB;
    update_pin_and_check_masks<Us, false>();
  } else if (!more_than_one(b)) {  // Single check
    undo_->check_mask = b;
    update_pin_and_check_masks<Us, true>();
  } else  // Double check
    undo_->check_mask = EmptyBB;

  if (KING_ATK[ksq] & enemy_or_empty) undo_->attacked = threatened<Us>();
}

}  // namespace Lyra
