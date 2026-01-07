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

// Undo state, used for do_move and undo_move
struct Undo {
  Piece  cap;
  Castle castling;
  Square ep;

  U8           rule50;
  U16          ply_from_null_;
  Move         mv;
  Zobrist::Key key;
  Zobrist::Key pawn_key;

  // Check mask: FullBB = No checks, EmptyBB = Double check, otherwise = Checkers
  // Attacked: EmptyBB = King cannot move, otherwise = squares attacked by enemy
  BB check_mask, diag_pin, hv_pin, attacked;

  Score psq;
  int   game_phase;
};

/******************************************\
|==========================================|
|               Board Struct               |
|==========================================|
\******************************************/

class Board {
 private:
  BB    pieceBB_[NPieceType];
  BB    colourBB_[NColour];
  Piece board_[NSquare];

  Colour     stm_;
  U16        ply_;
  CastleMask castling_mask_;

  Undo* undo_;
  Undo* history_;

  template <bool DoMove, Colour C>
  constexpr void set_piece(Piece pc, Square sq);
  template <bool DoMove, Colour C>
  constexpr void pop_piece(Square sq);
  template <bool DoMove, Colour C>
  constexpr void move_piece(Square src, Square dst);

  template <Colour Us>
  void update_masks();
  template <Colour Us, bool inCheck>
  constexpr void update_pin_and_check_masks();
  template <Colour Us>
  constexpr bool can_ep(Square ep);
  template <Colour Us>
  constexpr BB checkers();
  template <Colour Us>
  constexpr BB threatened();

 public:
  Board();
  ~Board();

  bool chess960;

  Board(const Board& board)      = delete;  // No Copying
  Board& operator=(const Board&) = delete;

  void set(const std::string& fen);
  void set(const Board& board);
  void reset();

  void        print() const;
  std::string fen() const;

  void do_move(Move move);
  template <Colour Us>
  void do_move(Move move);
  template <Colour Us>
  void undo_move();

  constexpr BB bb() const;
  constexpr BB bb(Colour c) const;
  constexpr BB bb(PieceType pt) const;
  constexpr BB bb(Piece pc) const;
  constexpr BB bb(Piece pc1, Piece pc2) const;

  constexpr Piece on(Square sq) const;
  template <Colour C>
  constexpr Square ksq() const;
  constexpr Colour stm() const;

  constexpr CastleMask castling_mask() const;

  constexpr Undo*         state();
  constexpr Undo*         state() const;
  constexpr Zobrist::Key* rep_table();

  Zobrist::Key          compute_key() const;
  Zobrist::Key          compute_pawn_key() const;
  std::pair<Score, int> compute_psq() const;

  Eval compute_raw_eval() const;
  Eval compute_incr_eval() const;

  bool is_draw() const;
  bool is_reps() const;
  bool in_check() const;
};

/******************************************\
|==========================================|
|           Board State Helpers            |
|==========================================|
\******************************************/

constexpr Undo*      Board::state() { return undo_; }
constexpr Undo*      Board::state() const { return undo_; }
constexpr Colour     Board::stm() const { return stm_; }
constexpr CastleMask Board::castling_mask() const { return castling_mask_; }

/******************************************\
|==========================================|
|          Board BitBoard Getters          |
|==========================================|
\******************************************/

constexpr BB Board::bb() const { return colourBB_[White] | colourBB_[Black]; }
constexpr BB Board::bb(Colour c) const { return colourBB_[c]; }
constexpr BB Board::bb(PieceType pt) const { return pieceBB_[pt]; }
constexpr BB Board::bb(Piece pc) const { return bb(colour_of(pc)) & bb(pt_of(pc)); }
constexpr BB Board::bb(Piece pc1, Piece pc2) const { return bb(pc1) | bb(pc2); }

/******************************************\
|==========================================|
|              Board Getters               |
|==========================================|
\******************************************/

constexpr Piece Board::on(Square sq) const { return board_[sq]; }
template <Colour C>
constexpr Square Board::ksq() const {
  return BBUtils::lsb(bb(make_piece(C, K)));
}

/******************************************\
|==========================================|
|            Piece Manipulation            |
|==========================================|
\******************************************/

template <bool DoMove, Colour C>
constexpr void Board::set_piece(Piece pc, Square sq) {
  colourBB_[C]        |= BBUtils::from(sq);
  pieceBB_[pt_of(pc)] |= BBUtils::from(sq);
  board_[sq]           = pc;

  if constexpr (!DoMove) return;

  undo_->key        ^= Zobrist::PIECE_KEYS[pc][sq];
  undo_->psq        += EvalUtils::PSQT[pc][sq];
  undo_->game_phase += EvalUtils::GamePhaseInc[pt_of(pc)];
}

template <bool DoMove, Colour C>
constexpr void Board::pop_piece(Square sq) {
  const Piece pc       = board_[sq];
  colourBB_[C]        &= ~BBUtils::from(sq);
  pieceBB_[pt_of(pc)] &= ~BBUtils::from(sq);
  board_[sq]           = NoPiece;

  if constexpr (!DoMove) return;

  undo_->key        ^= Zobrist::PIECE_KEYS[pc][sq];
  undo_->psq        -= EvalUtils::PSQT[pc][sq];
  undo_->game_phase -= EvalUtils::GamePhaseInc[pt_of(pc)];
}

template <bool DoMove, Colour C>
constexpr void Board::move_piece(Square src, Square dst) {
  const Piece pc       = board_[src];
  colourBB_[C]        ^= BBUtils::from(src) ^ BBUtils::from(dst);
  pieceBB_[pt_of(pc)] ^= BBUtils::from(src) ^ BBUtils::from(dst);
  board_[src]          = NoPiece;
  board_[dst]          = pc;

  if constexpr (!DoMove) return;

  undo_->key ^= Zobrist::PIECE_KEYS[pc][src] ^ Zobrist::PIECE_KEYS[pc][dst];
  undo_->psq += EvalUtils::PSQT[pc][dst] - EvalUtils::PSQT[pc][src];
}

/******************************************\
|==========================================|
|             Repetitions/Draw             |
|==========================================|
\******************************************/

// constexpr void Board::update_reps() {
//   Reps& r = rep_table_[undo_->rule50];
//   r.key   = undo_->key;
//   r.reps  = 0;
//
//   int end = std::min((U16)undo_->rule50, undo_->ply_from_null_);
//   if (end >= 4) {
//     for (int i = 2; i <= end; i += 2) {
//       Reps& prev = rep_table_[undo_->rule50 - i];
//       if (prev.key != r.key) continue;
//       r.reps = prev.reps ? -i : i;
//       break;
//     }
//   }
// }

}  // namespace Lyra
