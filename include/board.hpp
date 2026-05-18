#pragma once

#include "bitboard.hpp"
#include "castle.hpp"
#include "defs.hpp"
#include "move.hpp"
#include "utils.hpp"
#include "zobrist.hpp"

namespace Lyra {

/******************************************\
|==========================================|
|            Useful fen strings            |
|==========================================|
\******************************************/

constexpr std::string_view start_pos = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 ";

/******************************************\
|==========================================|
|                 Piece To                 |
|==========================================|
\******************************************/

struct PieceTo {
  Piece  pc;
  Square to;
};

/******************************************\
|==========================================|
|                Undo State                |
|==========================================|
\******************************************/

// Undo state, used for do_move and undo_move,
// stores variables that cannot be inferred when undoing a move
struct Undo {
  // Hash keys
  Key key;
  Key pawn_key;
  // Board variables
  Ply    ply; // Half move counter from Board::set position
  Move   move;
  Castle c_rights;
  U8     rule50; // Fifty move counter

  // Board variables that are not copied
  Square ep;
  Piece  cap;
  I8     reps;

  // Move generation masks
  BB check_mask, diag_pin, hv_pin, attacked;
};

/******************************************\
|==========================================|
|               Board Struct               |
|==========================================|
\******************************************/

class Board {
  NDArray<BB, NPieceType> piece_bbs_;
  NDArray<BB, NColour>    colour_bbs_;
  NDArray<Piece, NSquare> board_;

  Colour stm_;
  // Ply from the start of the game, which includes the half move clock in fen.
  Ply        gameply_;
  CastleMask castling_mask_;

  Undo             *undo_;
  std::vector<Undo> history_;

  // Move and Undo Move helper functions
  template <bool DoMove, Colour C>
  constexpr void set_piece(Piece pc, Square sq);
  template <bool DoMove, Colour C>
  constexpr void pop_piece(Square sq);
  template <bool DoMove, Colour C>
  constexpr void move_piece(Square src, Square dst);

  // Board masks update functions
  template <Colour Us>
  void update_masks();
  template <Colour Us, bool inCheck>
  constexpr void update_pin_and_check_masks();
  template <Colour Us>
  constexpr BB checkers();
  template <Colour Us>
  constexpr BB threatened();
  template <Colour Us>
  constexpr bool can_ep(Square ep);
  void           update_reps() const;

  // Functions that compute from scratch
  Eval                  compute_raw_eval() const;
  Key                   compute_key() const;
  Key                   compute_pawn_key() const;
  std::pair<Score, int> compute_psq() const; // (PSQT score, Game Phase)

public:
  Board();

  bool chess960;

  // No implicit copying
  Board(const Board &board)       = delete;
  Board &operator=(const Board &) = delete;

  // Setters
  void set(const std::string &fen);
  void copy(const Board &board);
  void reset();

  // IO functions
  void        print() const;
  std::string fen() const;

  // Move and Undo Move
  void do_move(Move move);
  template <Colour Us>
  void do_move(Move move);
  template <Colour Us>
  void undo_move();
  template <Colour Us>
  void do_null_move();
  template <Colour Us>
  void undo_null_move();

  // Bitboard getters
  constexpr BB bb() const { return colour_bbs_[White] | colour_bbs_[Black]; }
  constexpr BB bb(Colour c) const { return colour_bbs_[c]; }
  constexpr BB bb(PieceType pt) const { return piece_bbs_[pt]; }
  constexpr BB bb(PieceType pt1, PieceType pt2) const { return bb(pt1) | bb(pt2); }
  constexpr BB bb(Colour c, PieceType pt) const { return bb(c) & bb(pt); }
  constexpr BB bb(Colour c, PieceType pt1, PieceType pt2) const { return bb(c) & bb(pt1, pt2); }

  const NDArray<BB, NColour>    &colour_bbs() const { return colour_bbs_; }
  const NDArray<BB, NPieceType> &piece_bbs() const { return piece_bbs_; }

  // Getters
  constexpr Piece       on(Square sq) const { return board_[sq]; }
  constexpr Colour      stm() const { return stm_; }
  constexpr Undo *const undo() const { return undo_; }
  constexpr CastleMask  castle_mask() const { return castling_mask_; }
  constexpr Key         key() const { return undo_->key; }
  constexpr Key         pawn_key() const { return undo_->pawn_key; }

  template <Colour C>
  constexpr Square    ksq() const;
  constexpr PieceType moved(Move move) const;
  constexpr PieceType captured(Move move) const;
  constexpr PieceTo   piece_to(Move move) const;
  Eval                eval() const;

  // Movegen Helpers
  template <Colour Us, bool QueenSide>
  constexpr bool can_castle() const;

  // Movepick Helpers
  BB attackers_to(Square to, BB occ) const;
  template <Colour Us>
  bool is_legal(Move move) const;
  bool see(Move move, Eval threshold) const;

  // Game State functions
  bool is_draw(Ply ply) const;
  bool is_insufficient_material() const;
  bool in_check() const;
  template <Colour Us>
  bool gives_check(Move move) const;
  bool has_non_pawn_material(Colour us) const;
};

/******************************************\
|==========================================|
|              Board Getters               |
|==========================================|
\******************************************/

constexpr PieceType Board::moved(Move move) const { return pt_of(on(MoveUtils::src(move))); }

constexpr PieceType Board::captured(Move move) const {
  return MoveUtils::is_ep(move) ? P : pt_of(on(MoveUtils::dst(move)));
}
constexpr PieceTo Board::piece_to(Move move) const {
  return {on(MoveUtils::src(move)), MoveUtils::dst(move)};
}
template <Colour C>
constexpr Square Board::ksq() const {
  return BBUtils::lsb(bb(C, K));
}

/******************************************\
|==========================================|
|            Piece Manipulation            |
|==========================================|
\******************************************/

template <bool DoMove, Colour C>
constexpr void Board::set_piece(Piece pc, Square sq) {
  colour_bbs_[C] |= BBUtils::from(sq);
  piece_bbs_[pt_of(pc)] |= BBUtils::from(sq);
  board_[sq] = pc;

  if constexpr (!DoMove) return;

  if (pt_of(pc) == P) undo_->pawn_key ^= Zobrist::PIECE_KEYS[pc][sq];

  undo_->key ^= Zobrist::PIECE_KEYS[pc][sq];
}

template <bool DoMove, Colour C>
constexpr void Board::pop_piece(Square sq) {
  const Piece pc = board_[sq];
  colour_bbs_[C] &= ~BBUtils::from(sq);
  piece_bbs_[pt_of(pc)] &= ~BBUtils::from(sq);
  board_[sq] = NoPiece;

  if constexpr (!DoMove) return;

  if (pt_of(pc) == P) undo_->pawn_key ^= Zobrist::PIECE_KEYS[pc][sq];

  undo_->key ^= Zobrist::PIECE_KEYS[pc][sq];
}

template <bool DoMove, Colour C>
constexpr void Board::move_piece(Square src, Square dst) {
  const Piece pc = board_[src];
  colour_bbs_[C] ^= BBUtils::from(src) ^ BBUtils::from(dst);
  piece_bbs_[pt_of(pc)] ^= BBUtils::from(src) ^ BBUtils::from(dst);
  board_[src] = NoPiece;
  board_[dst] = pc;

  if constexpr (!DoMove) return;

  Key toggle = Zobrist::PIECE_KEYS[pc][src] ^ Zobrist::PIECE_KEYS[pc][dst];

  if (pt_of(pc) == P) undo_->pawn_key ^= toggle;
  undo_->key ^= toggle;
}

/******************************************\
|==========================================|
|             Castle/Ep Helpers            |
|==========================================|
\******************************************/

template <Colour Us, bool QueenSide>
constexpr bool Board::can_castle() const {
  constexpr Square kd = CastleMask::king_dst<Us>(QueenSide);
  constexpr Square rd = CastleMask::rook_dst<Us>(QueenSide);
  const Square     ks = ksq<Us>();
  const Square     rs = castling_mask_.rook_src<Us>(QueenSide);

  const BB occ      = bb();
  const BB attacked = undo_->attacked;
  const BB hv_pin   = undo_->hv_pin;

  const BB king_area = BTWN_BB[ks][kd] | from(kd);
  const BB rook_area = BTWN_BB[rs][rd] | from(rd);
  const BB occ_mask  = (king_area | rook_area) & ~(from(ks) | from(rs));

  return !(king_area & attacked) && !(occ_mask & occ) && !(from(rs) & hv_pin);
}

template <Colour Us>
constexpr bool Board::can_ep(Square ep) {
  using enum Direction;
  constexpr Colour    Them = ~Us;
  constexpr Direction Up   = Us == White ? N : S;

  const BB     ep_rank   = Us == White ? from(Rank5) : from(Rank4);
  const BB     king      = bb(Us, K);
  const BB     pawns     = bb(Us, P);
  const BB     enemy_rq  = bb(Them, R, Q);
  const BB     occ       = bb();
  const BB     ep_target = shift<~Up>(from(ep));
  const Square ksq       = Board::ksq<Us>();
  const BB     ep_w      = pawns & shift<E>(ep_target);
  const BB     ep_e      = pawns & shift<W>(ep_target);

  // If the enemy rook/queen sees the king after simulating the enpassant,
  // register the enpassant pin
  const bool has_attacker = PAWN_ATK[Them][ep] & pawns;
  const bool no_hv_pin    = !(ep_rank & king) || !(ep_rank & pawns) || !(ep_rank & enemy_rq);
  const bool ep_w_pin     = ep_w && ROOK_ATK[ksq][occ & ~(ep_target | ep_w)] & enemy_rq;
  const bool ep_e_pin     = ep_e && ROOK_ATK[ksq][occ & ~(ep_target | ep_e)] & enemy_rq;

  return has_attacker && (no_hv_pin || !(ep_w_pin || ep_e_pin));
}

} // namespace Lyra
