#pragma once

#include "bitboard.hpp"
#include "castle.hpp"
#include "defs.hpp"
#include "eval.hpp"
#include "move.hpp"
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
|                Undo State                |
|==========================================|
\******************************************/

// Undo state, used for do_move and undo_move,
// stores variables that cannot be inferred when undoing a move
struct Undo {
  // Hash keys
  Key key;
  Key pawn_key;
  // Piece square table scores
  Score psq;
  int   game_phase;
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
  BB    pieceBB_[NPieceType];
  BB    colourBB_[NColour];
  Piece board_[NSquare];

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
  Eval compute_raw_eval() const;
  Key  compute_key() const;
  Key  compute_pawn_key() const;

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
  constexpr BB bb() const;
  constexpr BB bb(Colour c) const;
  constexpr BB bb(PieceType pt) const;
  constexpr BB bb(PieceType pt1, PieceType pt2) const;
  constexpr BB bb(Colour c, PieceType pt) const;
  constexpr BB bb(Colour c, PieceType pt1, PieceType pt2) const;

  // Getters
  template <Colour C>
  constexpr Square      ksq() const;
  constexpr Piece       on(Square sq) const;
  constexpr PieceType   moved(Move move) const;
  constexpr PieceType   captured(Move move) const;
  constexpr Colour      stm() const;
  constexpr Undo *const undo() const;
  Eval                  eval() const;
  std::pair<Score, int> compute_psq() const; // (PSQT score, Game Phase)
  constexpr CastleMask  castle_mask() const;
  constexpr Key         key() const;

  // Movegen Helpers
  template <Colour Us, bool QueenSide>
  constexpr bool can_castle() const;

  // Movepick Helpers
  BB   attackers_to(Square to, BB occ) const;
  bool see(Move move, Eval threshold) const;

  // Game State functions
  bool is_draw(Ply ply) const;
  bool in_check() const;
  bool has_non_pawn_material(Colour us) const;
};

/******************************************\
|==========================================|
|           Board State Helpers            |
|==========================================|
\******************************************/

constexpr Undo *const Board::undo() const { return undo_; }
constexpr Colour      Board::stm() const { return stm_; }
constexpr CastleMask  Board::castle_mask() const { return castling_mask_; }
constexpr Key         Board::key() const { return undo_->key; }

/******************************************\
|==========================================|
|          Board BitBoard Getters          |
|==========================================|
\******************************************/

constexpr BB Board::bb() const { return colourBB_[White] | colourBB_[Black]; }
constexpr BB Board::bb(Colour c) const { return colourBB_[c]; }
constexpr BB Board::bb(PieceType pt) const { return pieceBB_[pt]; }
constexpr BB Board::bb(PieceType pt1, PieceType pt2) const { return bb(pt1) | bb(pt2); }
constexpr BB Board::bb(Colour c, PieceType pt) const { return bb(c) & bb(pt); }
constexpr BB Board::bb(Colour c, PieceType pt1, PieceType pt2) const {
  return bb(c) & bb(pt1, pt2);
}

/******************************************\
|==========================================|
|              Board Getters               |
|==========================================|
\******************************************/

constexpr Piece     Board::on(Square sq) const { return board_[sq]; }
constexpr PieceType Board::moved(Move move) const { return pt_of(on(MoveUtils::src(move))); }
constexpr PieceType Board::captured(Move move) const {
  return MoveUtils::is_ep(move) ? P : pt_of(on(MoveUtils::dst(move)));
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
  colourBB_[C] |= BBUtils::from(sq);
  pieceBB_[pt_of(pc)] |= BBUtils::from(sq);
  board_[sq] = pc;

  if constexpr (!DoMove) return;

  undo_->key ^= Zobrist::PIECE_KEYS[pc][sq];
  undo_->psq += EvalUtils::PSQT[pc][sq];
  undo_->game_phase += EvalUtils::GamePhaseInc[pt_of(pc)];
}

template <bool DoMove, Colour C>
constexpr void Board::pop_piece(Square sq) {
  const Piece pc = board_[sq];
  colourBB_[C] &= ~BBUtils::from(sq);
  pieceBB_[pt_of(pc)] &= ~BBUtils::from(sq);
  board_[sq] = NoPiece;

  if constexpr (!DoMove) return;

  undo_->key ^= Zobrist::PIECE_KEYS[pc][sq];
  undo_->psq -= EvalUtils::PSQT[pc][sq];
  undo_->game_phase -= EvalUtils::GamePhaseInc[pt_of(pc)];
}

template <bool DoMove, Colour C>
constexpr void Board::move_piece(Square src, Square dst) {
  const Piece pc = board_[src];
  colourBB_[C] ^= BBUtils::from(src) ^ BBUtils::from(dst);
  pieceBB_[pt_of(pc)] ^= BBUtils::from(src) ^ BBUtils::from(dst);
  board_[src] = NoPiece;
  board_[dst] = pc;

  if constexpr (!DoMove) return;

  undo_->key ^= Zobrist::PIECE_KEYS[pc][src] ^ Zobrist::PIECE_KEYS[pc][dst];
  undo_->psq += EvalUtils::PSQT[pc][dst] - EvalUtils::PSQT[pc][src];
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
