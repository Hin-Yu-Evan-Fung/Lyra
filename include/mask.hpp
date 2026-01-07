#include "bitboard.hpp"
#include "board.hpp"
#include "defs.hpp"

namespace Lyra {

using namespace BBUtils;

template <Colour Us>
constexpr BB Board::threatened() {
  constexpr Colour Them   = ~Us;
  constexpr Piece  Pawn   = make_piece(Them, P);
  constexpr Piece  Knight = make_piece(Them, N);
  constexpr Piece  Bishop = make_piece(Them, B);
  constexpr Piece  Rook   = make_piece(Them, R);
  constexpr Piece  Queen  = make_piece(Them, Q);
  constexpr Piece  King   = make_piece(Us, K);

  const BB occ            = bb() ^ bb(King);

  BB threatened           = pawn_attack_bb<Them>(bb(Pawn)) | KING_ATK[ksq<Them>()];
  bitloop(bb(Knight), [&](Square src) { threatened |= KNIGHT_ATK[src]; });
  bitloop(bb(Bishop, Queen), [&](Square src) { threatened |= BISHOP_ATK[src][occ]; });
  bitloop(bb(Rook, Queen), [&](Square src) { threatened |= ROOK_ATK[src][occ]; });

  return threatened;
}

template <Colour Us>
constexpr BB Board::checkers() {
  constexpr Colour Them   = ~Us;
  constexpr Piece  Pawn   = make_piece(Them, P);
  constexpr Piece  Knight = make_piece(Them, N);
  constexpr Piece  Bishop = make_piece(Them, B);
  constexpr Piece  Rook   = make_piece(Them, R);
  constexpr Piece  Queen  = make_piece(Them, Q);

  const BB     occ        = bb();
  const Square ksq        = Board::ksq<Us>();

  return (PAWN_ATK[Us][ksq] & bb(Pawn)) | (KNIGHT_ATK[ksq] & bb(Knight)) | (BISHOP_ATK[ksq][occ] & bb(Bishop, Queen)) |
         (ROOK_ATK[ksq][occ] & bb(Rook, Queen));
}

template <Colour Us, bool inCheck>
constexpr void Board::update_pin_and_check_masks() {
  using enum Direction;
  constexpr Colour Them   = ~Us;
  constexpr Piece  Bishop = make_piece(Them, B);
  constexpr Piece  Rook   = make_piece(Them, R);
  constexpr Piece  Queen  = make_piece(Them, Q);

  const BB     their_occ  = bb(Them);
  const BB     our_occ    = bb(Us);
  const Square ksq        = Board::ksq<Us>();
  BB           diag_pin = EmptyBB, hv_pin = EmptyBB, check_mask = EmptyBB;
  BB           pin_mask;

  // clang-format off

    BB pinners = BISHOP_ATK[ksq][their_occ] & bb(Bishop, Queen);
    bitloop(pinners, [&](Square atk) {
        pin_mask = BTWN_BB[ksq][atk] | from(atk);
        switch (popcount(pin_mask & our_occ)) {
        case 0: if constexpr (inCheck) check_mask |= pin_mask; break;
        case 1: diag_pin |= pin_mask;
        }
    });

    pinners = ROOK_ATK[ksq][their_occ] & bb(Rook, Queen);
    bitloop(pinners, [&](Square atk) {
        pin_mask = BTWN_BB[ksq][atk] | from(atk);
        switch (popcount(pin_mask & our_occ)) {
        case 0: if constexpr (inCheck) check_mask |= pin_mask; break;
        case 1: hv_pin |= pin_mask;
        }
    });

  // clang-format on

  undo_->diag_pin = diag_pin;
  undo_->hv_pin   = hv_pin;
  if constexpr (inCheck) undo_->check_mask |= check_mask;
}

template <Colour Us>
constexpr bool Board::can_ep(Square ep) {
  using enum Direction;
  constexpr Colour    Them   = ~Us;
  constexpr Direction Up     = Us == White ? N : S;
  constexpr Piece     ERook  = make_piece(Them, R);
  constexpr Piece     EQueen = make_piece(Them, Q);

  const BB     ep_rank       = Us == White ? from(Rank5) : from(Rank4);
  const BB     king          = bb(make_piece(Us, K));
  const BB     pawns         = bb(make_piece(Us, P));
  const BB     enemy_rq      = bb(ERook, EQueen);
  const BB     occ           = bb();
  const BB     ep_target     = shift<~Up>(from(ep));
  const Square ksq           = Board::ksq<Us>();

  BB ep_w                    = pawns & shift<E>(ep_target);
  BB ep_e                    = pawns & shift<W>(ep_target);

  // If the enemy rook/queen sees the king after simulating the enpassant, register the enpassant pin
  bool has_attacker = PAWN_ATK[Them][ep] & pawns;
  bool no_hv_pin    = !(ep_rank & king) || !(ep_rank & pawns) || !(ep_rank & enemy_rq);
  bool ep_w_pin     = ep_w && ROOK_ATK[ksq][occ & ~(ep_target | ep_w)] & enemy_rq;
  bool ep_e_pin     = ep_e && ROOK_ATK[ksq][occ & ~(ep_target | ep_e)] & enemy_rq;

  return has_attacker && (no_hv_pin || !(ep_w_pin || ep_e_pin));
}

template <Colour Us>
void Board::update_masks() {
  constexpr Piece Rook           = make_piece(Us, R);
  const Square    ksq            = Board::ksq<Us>();
  const BB        enemy_or_empty = ~bb(Us) | bb(Rook);
  BB              b              = checkers<Us>();

  if (!b) {
    undo_->check_mask = FullBB;
    update_pin_and_check_masks<Us, false>();
  } else if (!more_than_one(b)) {
    undo_->check_mask = b;
    update_pin_and_check_masks<Us, true>();
  } else
    undo_->check_mask = EmptyBB;

  if (KING_ATK[ksq] & enemy_or_empty) undo_->attacked = threatened<Us>();
}

}  // namespace Lyra
