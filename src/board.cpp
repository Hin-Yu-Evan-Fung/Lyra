#include "board.hpp"

#include <cassert>
#include <cstring>
#include <ios>
#include <iostream>
#include <sstream>

#include "bitboard.hpp"
#include "castling.hpp"
#include "defs.hpp"
#include "eval.hpp"
#include "mask.hpp"
#include "utils.hpp"
#include "zobrist.hpp"

namespace Lyra {

/******************************************\
|==========================================|
|              Input/Output                |
|==========================================|
\******************************************/

Board::Board() {
  history_ = new Undo[MAX_DEPTH];
  reset();
}

Board::~Board() { delete[] history_; }

void Board::reset() {
  half_mv_           = 0;
  state_             = history_;
  state_->castling   = NoCastle;
  state_->ep         = NoSquare;
  state_->fifty_mv   = 0;
  state_->psq        = {};
  state_->game_phase = 0;

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
      Piece pc = char2piece(c);
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
      rsq = relative(s, H1);
      while (on(rsq) != rook)
        --rsq;
      castling = CastleMask::get_mask(s, false);
      castling_mask_.add_rights(ksq, rsq, castling);
    } else if (upper == 'Q') {
      rsq = relative(s, A1);
      while (on(rsq) != rook)
        ++rsq;
      castling = CastleMask::get_mask(s, true);
      castling_mask_.add_rights(ksq, rsq, castling);
    } else if (upper >= 'A' && upper <= 'H') {
      rsq      = relative(s, make_square(char2file(upper), Rank1));
      castling = CastleMask::get_mask(s, ksq > rsq);
      castling_mask_.add_rights(ksq, rsq, castling);
    }

    state_->castling |= castling;
  }

  // 4. Parse enpassant
  ss >> std::skipws >> part;
  state_->ep = NoSquare;
  if (part.length() == 2) { state_->ep = str2sq(part); }

  int fifty_mv = 0, full_mv = 1;
  ss >> std::skipws >> fifty_mv;
  ss >> std::skipws >> full_mv;

  state_->fifty_mv = I8(fifty_mv);
  half_mv_         = I8(full_mv - 1) * 2 + I8(stm_);

  state_->key      = compute_key();

  // Basic board legality checks
  if (ksq<White>() == NoSquare) throw std::invalid_argument("Invalid fen! White king is not on the board!");
  if (ksq<Black>() == NoSquare) throw std::invalid_argument("Invalid fen! Black king is not on the board!");

  stm_ == White ? update_masks<White>() : update_masks<Black>();
}

void Board::print() const {
  printf("\n     +---+---+---+---+---+---+---+---+\n");

  for (Rank r = Rank8; r >= Rank1; --r) {
    printf(" %c   |", to_char(r));
    for (File f = FileA; f <= FileH; ++f)
      printf(" %c |", to_char(on(make_square(f, r))));

    printf("\n     +---+---+---+---+---+---+---+---+\n");
  }
  printf("\n       A   B   C   D   E   F   G   H\n\n");
  printf("Fen: %s\n", fen().c_str());
  printf("Side to move: %s\n", stm_ == White ? "White" : "Black");
  printf("Castling Rights: %s\n", castling_mask_.to_str(state_->castling).c_str());
  printf("Enpassant Square: %s\n", to_str(state_->ep).c_str());
  printf("Hash Key: 0x%lx\n", state_->key);
  printf("Incremental PSQ: %d\n", eval_incr());
  printf("Real PSQ: %d\n", eval_raw());
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
        out << to_char(pc);
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
  out << " " << castling_mask_.to_str(state_->castling);
  out << " " << (state_->ep != NoSquare ? to_str(state_->ep) : "-");
  out << " " << int(state_->fifty_mv);
  out << " " << half_mv_ / 2 + 1;

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
  if (state_->ep != NoSquare) key ^= Zobrist::EP_KEYS[file_of(state_->ep)];

  key ^= Zobrist::CASTLE_KEYS[state_->castling];

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

Eval Board::eval_raw() const {
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

Eval Board::eval_incr() const {
  Eval raw = state_->psq.to_eval(state_->game_phase);
  return stm_ == White ? raw : -raw;
}

}  // namespace Lyra
