#include "board.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <ios>
#include <iostream>
#include <print>
#include <sstream>

#include "bitboard.hpp"
#include "castle.hpp"
#include "defs.hpp"
#include "eval.hpp"
#include "mask.hpp"
#include "movegen.hpp"
#include "utils.hpp"
#include "zobrist.hpp"

namespace Lyra {

/******************************************\
|==========================================|
|              Input/Output                |
|==========================================|
\******************************************/

Board::Board() {
  history_ = new Undo[MaxPly];
  reset();
}

Board::~Board() { delete[] history_; }

void Board::reset() {
  chess960              = false;
  ply_                  = 0;
  undo_                 = history_;
  undo_->ply_from_null_ = 0;
  undo_->castling       = NoCastle;
  undo_->ep             = NoSquare;
  undo_->rule50         = 0;
  undo_->psq            = {};
  undo_->game_phase     = 0;

  memset(pieceBB_, BBUtils::EmptyBB, sizeof(pieceBB_));
  memset(colourBB_, BBUtils::EmptyBB, sizeof(colourBB_));
  memset(board_, NoPiece, sizeof(board_));

  castling_mask_.reset();
}

void Board::set(const std::string& fen) {
  std::istringstream ss(fen);
  std::string        part;

  int    file = FileA;
  int    rank = Rank8;
  Square sq;

  reset();

  // 1. Parse pieces
  ss >> std::skipws >> part;
  for (char c : part) {
    sq = make_square(File(file), Rank(rank));

    if (std::isalpha(c)) {
      Piece pc = IOUtils::parse_piece(c);
      colour_of(pc) == White ? set_piece<true, White>(pc, sq) : set_piece<true, Black>(pc, sq);
      file++;
    } else if (std::isdigit(c)) {
      file += c - '0';
    } else if (c == '/') {
      file = 0;
      rank--;
    }
  }

  // 2. Parse side to move
  ss >> std::skipws >> part;
  stm_ = part[0] == 'w' ? White : Black;

  // 3. Parse castling
  ss >> std::skipws >> part;
  Castle castling = NoCastle;
  for (char c : part) {
    Colour s     = std::isupper(c) ? White : Black;
    char   upper = std::toupper(c);

    Piece  rook  = make_piece(s, R);
    Square ksq   = s == White ? Board::ksq<White>() : Board::ksq<Black>();
    Square rsq;

    if (upper == 'K') {
      rsq = relative_sq(s, H1);
      while (on(rsq) != rook)
        --rsq;
      castling = CastleMask::get_mask(s, false);
      castling_mask_.add_rights(ksq, rsq, castling);
    } else if (upper == 'Q') {
      rsq = relative_sq(s, A1);
      while (on(rsq) != rook)
        ++rsq;
      castling = CastleMask::get_mask(s, true);
      castling_mask_.add_rights(ksq, rsq, castling);
    } else if (upper >= 'A' && upper <= 'H') {
      rsq      = relative_sq(s, make_square(IOUtils::parse_file(upper), Rank1));
      castling = CastleMask::get_mask(s, ksq > rsq);
      castling_mask_.add_rights(ksq, rsq, castling);
    }

    undo_->castling |= castling;
  }

  // 4. Parse enpassant
  ss >> std::skipws >> part;
  undo_->ep = NoSquare;
  if (part.length() == 2) { undo_->ep = IOUtils::parse_sq(part); }

  int fifty_mv = 0, full_mv = 1;
  ss >> std::skipws >> fifty_mv;
  ss >> std::skipws >> full_mv;

  undo_->rule50 = I8(fifty_mv);
  ply_          = I8(full_mv - 1) * 2 + I8(stm_);
  undo_->key    = compute_key();

  // Basic board legality checks
  if (ksq<White>() == NoSquare) throw std::invalid_argument("Invalid fen! White king is not on the board!");
  if (ksq<Black>() == NoSquare) throw std::invalid_argument("Invalid fen! Black king is not on the board!");

  stm_ == White ? update_masks<White>() : update_masks<Black>();
}

void Board::set(const Board& board) {
  chess960       = board.chess960;
  ply_           = board.ply_;
  castling_mask_ = board.castling_mask_;
  stm_           = board.stm_;
  std::copy_n(board.pieceBB_, NPieceType, pieceBB_);
  std::copy_n(board.colourBB_, NColour, colourBB_);
  std::copy_n(board.board_, NSquare, board_);
  std::copy_n(board.history_, board.undo_->ply_from_null_ + 1, history_);
  undo_ = history_ + board.undo_->ply_from_null_;
}

void Board::print() const {
  std::println("\n     +---+---+---+---+---+---+---+---+");

  for (Rank r = Rank8; r >= Rank1; --r) {
    std::print(" {}   |", IOUtils::format_rank(r));
    for (File f = FileA; f <= FileH; ++f)
      std::print(" {} |", IOUtils::format_piece(on(make_square(f, r))));

    std::println("\n     +---+---+---+---+---+---+---+---+");
  }
  std::println("\n       A   B   C   D   E   F   G   H\n");
  std::println("Fen: {}", fen());
  std::println("Side to move: {}", stm_ == White ? "White" : "Black");
  std::println("Castling Rights: {}", castling_mask_.to_str(undo_->castling).c_str());
  std::println("Enpassant Square: {}", IOUtils::format_sq(undo_->ep).c_str());
  std::println("Hash Key: {:#X}", undo_->key);
  std::println("Incremental PSQ: {}", compute_incr_eval());
  std::println("Real PSQ: {}", compute_raw_eval());
  std::println("Chess960: {}", chess960 ? "true" : "false");
  std::println("Ply from null: {}", undo_->ply_from_null_);
}

std::string Board::fen() const {
  std::ostringstream out;
  Square             sq;
  Piece              pc;

  for (Rank r = Rank8; r >= Rank1; --r) {
    int empty_count = 0;
    for (File f = FileA; f <= FileH; ++f) {
      sq = make_square(f, r);
      pc = on(sq);
      if (pc != NoPiece) {
        if (empty_count != 0) out << empty_count;
        out << IOUtils::format_piece(pc);
        empty_count = 0;
      } else {
        empty_count += 1;
      }
    }

    if (empty_count != 0) out << empty_count;
    if (r != Rank1)  // Move the piece back
      out << '/';
  }

  out << " " << (stm_ == Colour::White ? "w" : "b");
  out << " " << castling_mask_.to_str(undo_->castling);
  out << " " << (undo_->ep != NoSquare ? IOUtils::format_sq(undo_->ep) : "-");
  out << " " << int(undo_->rule50);
  out << " " << ply_ / 2 + 1;

  return out.str();
}

/******************************************\
|==========================================|
|                 Hashing                  |
|==========================================|
\******************************************/

Zobrist::Key Board::compute_key() const {
  Zobrist::Key key = 0;

  for (Square sq = A1; sq <= H8; ++sq) {
    Piece pc = on(sq);
    if (pc != NoPiece) key ^= Zobrist::PIECE_KEYS[pc][sq];
  }

  if (stm_ == Black) key ^= Zobrist::SIDE_KEY;
  if (undo_->ep != NoSquare) key ^= Zobrist::EP_KEYS[file_of(undo_->ep)];

  key ^= Zobrist::CASTLE_KEYS[undo_->castling];

  return key;
}

Zobrist::Key Board::compute_pawn_key() const {
  Zobrist::Key key = 0;

  for (Square sq = A1; sq <= H8; ++sq) {
    Piece pc = on(sq);
    if (pt_of(pc) == P) key ^= Zobrist::PIECE_KEYS[pc][sq];
  }

  return key;
}

/******************************************\
|==========================================|
|                  Score                   |
|==========================================|
\******************************************/

Eval Board::compute_raw_eval() const {
  Score score{};
  int   game_phase = 0;

  for (Square sq = A1; sq <= H8; ++sq) {
    Piece pc = on(sq);
    if (pc == NoPiece) continue;

    score      += EvalUtils::PSQT[pc][sq];
    game_phase += EvalUtils::GamePhaseInc[pt_of(pc)];
  }

  Eval raw = score.to_eval(game_phase);
  return stm_ == White ? raw : -raw;
}

Eval Board::compute_incr_eval() const {
  Eval raw = undo_->psq.to_eval(undo_->game_phase);
  return stm_ == White ? raw : -raw;
}

/******************************************\
|==========================================|
|                  Score                   |
|==========================================|
\******************************************/

bool Board::is_draw() const {
  bool not_checkmate = (undo_->check_mask == FullBB || list_moves(*this).size());
  if (undo_->rule50 >= Rule50Ply && not_checkmate) return true;
  return is_reps();
}

bool Board::is_reps() const {
  int end = std::min((U16)undo_->rule50, undo_->ply_from_null_);
  if (end >= 4)
    for (int i = 2, reps = 0; i <= end; i += 2)
      if (undo_->key == (undo_ - i)->key && ++reps >= 2) return true;
  return false;
}

bool Board::in_check() const { return undo_->check_mask != FullBB; }

}  // namespace Lyra
