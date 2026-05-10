#include "board.hpp"

#include "bitboard.hpp"
#include "castle.hpp"
#include "defs.hpp"
#include "eval.hpp"
#include "mask.hpp"
#include "movegen.hpp"
#include "utils.hpp"
#include "zobrist.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <ios>
#include <iostream>
#include <print>
#include <sstream>

namespace Lyra {

/******************************************\
|==========================================|
|              Input/Output                |
|==========================================|
\******************************************/

Board::Board()
    : history_(MaxPly) {
  reset();
}

void Board::reset() {
  chess960          = false;
  gameply_          = 0;
  undo_             = history_.data();
  undo_->ply        = 0;
  undo_->c_rights   = NoCastle;
  undo_->ep         = NoSquare;
  undo_->rule50     = 0;
  undo_->psq        = {};
  undo_->game_phase = 0;

  memset(pieceBB_, BBUtils::EmptyBB, sizeof(pieceBB_));
  memset(colourBB_, BBUtils::EmptyBB, sizeof(colourBB_));
  memset(board_, NoPiece, sizeof(board_));

  castling_mask_.reset();
}

void Board::set(const std::string &fen) {
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
      Piece pc = parse_piece(c);
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

    Piece  rook = make_piece(s, R);
    Square ksq  = s == White ? Board::ksq<White>() : Board::ksq<Black>();
    Square rsq;

    if (upper == 'K') {
      rsq = relative_sq(s, H1);
      while (on(rsq) != rook) --rsq;
      castling = CastleMask::get_mask(s, false);
      castling_mask_.add_rights(ksq, rsq, castling);
    } else if (upper == 'Q') {
      rsq = relative_sq(s, A1);
      while (on(rsq) != rook) ++rsq;
      castling = CastleMask::get_mask(s, true);
      castling_mask_.add_rights(ksq, rsq, castling);
    } else if (upper >= 'A' && upper <= 'H') {
      rsq      = relative_sq(s, make_square(parse_file(upper), Rank1));
      castling = CastleMask::get_mask(s, ksq > rsq);
      castling_mask_.add_rights(ksq, rsq, castling);
    }

    undo_->c_rights |= castling;
  }

  // 4. Parse enpassant
  ss >> std::skipws >> part;
  undo_->ep = NoSquare;
  if (part.length() == 2) {
    undo_->ep = parse_sq(part);
  }

  int fifty_mv = 0, full_mv = 1;
  ss >> std::skipws >> fifty_mv;
  ss >> std::skipws >> full_mv;

  undo_->rule50   = I8(fifty_mv);
  gameply_        = I8(full_mv - 1) * 2 + I8(stm_);
  undo_->key      = compute_key();
  undo_->pawn_key = compute_pawn_key();

  // Basic board legality checks
  if (ksq<White>() == NoSquare)
    throw std::invalid_argument("Invalid fen! White king is not on the board!");
  if (ksq<Black>() == NoSquare)
    throw std::invalid_argument("Invalid fen! Black king is not on the board!");

  stm_ == White ? update_masks<White>() : update_masks<Black>();
}

void Board::copy(const Board &board) {
  chess960       = board.chess960;
  gameply_       = board.gameply_;
  castling_mask_ = board.castling_mask_;
  stm_           = board.stm_;
  std::copy_n(board.pieceBB_, NPieceType, pieceBB_);
  std::copy_n(board.colourBB_, NColour, colourBB_);
  std::copy_n(board.board_, NSquare, board_);
  std::copy_n(board.history_.begin(), board.undo_->ply + 1, history_.begin());
  undo_ = history_.data() + board.undo_->ply;
}

void Board::print() const {
  std::println("\n     +---+---+---+---+---+---+---+---+");

  for (Rank r = Rank8; r >= Rank1; --r) {
    std::print(" {}   |", format_rank(r));
    for (File f = FileA; f <= FileH; ++f) std::print(" {} |", format_piece(on(make_square(f, r))));

    std::println("\n     +---+---+---+---+---+---+---+---+");
  }
  std::println("\n       A   B   C   D   E   F   G   H\n");
  std::println("Fen: {}", fen());
  std::println("Side to move: {}", stm_ == White ? "White" : "Black");
  std::println("Castling Rights: {}", castling_mask_.to_str(undo_->c_rights));
  std::println("Enpassant Square: {}", format_sq(undo_->ep));
  std::println("Hash Key: {:#X}", undo_->key);
  std::println("Chess960: {}", chess960 ? "true" : "false");
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
        out << format_piece(pc);
        empty_count = 0;
      } else {
        empty_count += 1;
      }
    }

    if (empty_count != 0) out << empty_count;
    if (r != Rank1) // Move the piece back
      out << '/';
  }

  out << " " << (stm_ == Colour::White ? "w" : "b");
  out << " " << castling_mask_.to_str(undo_->c_rights);
  out << " " << (undo_->ep != NoSquare ? format_sq(undo_->ep) : "-");
  out << " " << int(undo_->rule50);
  out << " " << gameply_ / 2 + 1;

  return out.str();
}

/******************************************\
|==========================================|
|                 Hashing                  |
|==========================================|
\******************************************/

Key Board::compute_key() const {
  Key key = 0;

  for (Square sq = A1; sq <= H8; ++sq) {
    Piece pc = on(sq);
    if (pc != NoPiece) key ^= Zobrist::PIECE_KEYS[pc][sq];
  }

  if (stm_ == Black) key ^= Zobrist::SIDE_KEY;
  if (undo_->ep != NoSquare) key ^= Zobrist::EP_KEYS[file_of(undo_->ep)];

  key ^= Zobrist::CASTLE_KEYS[undo_->c_rights];

  return key;
}

Key Board::compute_pawn_key() const {
  Key key = 0;

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

    score += EvalUtils::PSQT[pc][sq];
    game_phase += EvalUtils::GamePhaseInc[pt_of(pc)];
  }

  Eval raw = score.to_eval(game_phase);
  return stm_ == White ? raw : -raw;
}

Eval Board::eval() const {
  Eval raw = undo_->psq.to_eval(undo_->game_phase);
  return stm_ == White ? raw : -raw;
}

/******************************************\
|==========================================|
|                  Score                   |
|==========================================|
\******************************************/

bool Board::is_draw(Ply ply) const {
  if (is_insufficient_material()) return true;
  if (undo_->rule50 >= Rule50Ply) return true;
  return undo_->reps && undo_->reps < ply;
}

void Board::update_reps() const {
  undo_->reps = 0;
  int end     = std::min((U16)undo_->rule50, undo_->ply);
  if (end >= 4)
    for (int i = 2; i <= end; i += 2) {
      Undo *prev = undo_ - i;
      if (undo_->key == prev->key) {
        undo_->reps = prev->reps ? -i : i;
        break;
      }
    }
}

bool Board::is_insufficient_material() const {
  unsigned n_pieces       = popcount(bb());
  unsigned n_white_pieces = popcount(bb(White));
  unsigned n_black_pieces = popcount(bb(Black));
  BB       knights        = bb(N);
  BB       bishops        = bb(B);

  switch (n_pieces) {
  case 2: return true;
  case 3: return bb(N) | bb(B);
  case 4:
    if (bb(P) | bb(R) | bb(Q)) return false;
    if (n_white_pieces == n_black_pieces) return true;
    if (knights && !bishops) return true;
    if (!knights && bishops)
      return ((bishops & WhiteSqBB) == bishops) || ((bishops & BlackSqBB) == bishops);
    return false;
  default: return false;
  }

  return false;
}

bool Board::in_check() const { return undo_->check_mask != FullBB; }

bool Board::has_non_pawn_material(Colour us) const { return bb(us) ^ bb(us, K) ^ bb(us, P); }

} // namespace Lyra
