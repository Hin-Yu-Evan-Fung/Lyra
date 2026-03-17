#include "movepick.hpp"

#include "bitboard.hpp"
#include "defs.hpp"
#include "move.hpp"
#include "movegen.hpp"

#include <algorithm>
#include <iterator>

namespace Lyra {

constexpr Eval PIECE_VALS[NPieceType] = {100, 200, 300, 400, 500, 0};

template <Colour Us>
MovePicker<Us>::MovePicker(MPType type, const Board &board, MOStats mostats, Move tt_move,
                           Depth depth)
    : board_(board)
    , killer_(*mostats.killer)
    , history_(mostats.hist)
    , tt_move_(tt_move)
    , depth_(depth) {

  switch (type) {
  case MPType::Main: stage_ = MAIN_TT; break;
  case MPType::QSearch: stage_ = QSEARCH_TT; break;
  }

  if (type == MPType::QSearch && !is_capture(tt_move_)) {
    tt_move_ = NoMove;
  }

  if (killer_[0] == tt_move_ || is_capture(killer_[0]) || !board.is_legal<Us>(killer_[0]))
    killer_[0] = NoMove;
  if (killer_[1] == tt_move_ || is_capture(killer_[1]) || !board.is_legal<Us>(killer_[1]))
    killer_[1] = NoMove;
}

/******************************************\
|==========================================|
|                  Helpers                 |
|==========================================|
\******************************************/

template <Colour Us>
size_t MovePicker<Us>::best_idx(size_t start, size_t end) {
  return std::distance(scores_, std::max_element(scores_ + start, scores_ + end));
}

template <Colour Us>
void MovePicker<Us>::swap(size_t idx1, size_t idx2) {
  std::swap(moves_[idx1], moves_[idx2]);
  std::swap(scores_[idx1], scores_[idx2]);
}

template <Colour Us>
Move MovePicker<Us>::pop_front() {
  start_ptr_--;
  swap(best_idx(0, start_ptr_ + 1), start_ptr_);
  return moves_[start_ptr_];
}

template <Colour Us>
Move MovePicker<Us>::pop_back() {
  end_ptr_++;
  swap(best_idx(end_ptr_, MaxMoves), end_ptr_);
  return moves_[end_ptr_];
}

/******************************************\
|==========================================|
|                Score Move                |
|==========================================|
\******************************************/

template <Colour Us>
Eval MovePicker<Us>::score_cap(Move move) {
  // MVV LVA
  PieceType attacker = pt_of(board_.on(MoveUtils::src(move)));
  PieceType victim   = is_ep(move) ? P : pt_of(board_.on(MoveUtils::dst(move)));

  Eval mvv_lva = PIECE_VALS[victim] + 6 - PIECE_VALS[attacker] / 100;
  return mvv_lva;
}

template <Colour Us>
Eval MovePicker<Us>::score_quiet(Move move) {
  const PieceFromTo &p = piece_from_to(board_, move);
  return (*history_)[p.pc][p.to];
}

/******************************************\
|==========================================|
|             Move Generation              |
|==========================================|
\******************************************/

template <Colour Us>
void MovePicker<Us>::gen_score_cap() {
  start_ptr_ = 0;
  end_ptr_   = MaxMoves - 1;
  enum_moves<Us, GenCap>(board_, [&](Move move) {
    if (move == tt_move_) return;

    if (board_.see(move, EvalDraw)) {
      moves_[start_ptr_]    = move;
      scores_[start_ptr_++] = score_cap(move);
    } else {
      moves_[end_ptr_]    = move;
      scores_[end_ptr_--] = score_cap(move);
    }
  });
}

template <Colour Us>
void MovePicker<Us>::gen_score_quiet() {
  start_ptr_ = 0;

  enum_moves<Us, GenQuiet>(board_, [&](Move move) {
    if (move == tt_move_ || move == killer_[0] || move == killer_[1]) return;

    moves_[start_ptr_]    = move;
    scores_[start_ptr_++] = score_quiet(move);
  });
}

/******************************************\
|==========================================|
|            Next Move Function            |
|==========================================|
\******************************************/

template <Colour Us>
Move MovePicker<Us>::next() {
  switch (stage_) {
  case MAIN_TT:
  case QSEARCH_TT:
    ++stage_;
    if (tt_move_) return tt_move_;
    return next();
  case INIT_CAP:
  case QSEARCH_INIT:
    gen_score_cap();
    ++stage_;
    return next();
  case GOOD_CAP:
    if (peek_front()) return pop_front();
    ++stage_;
    return next();
  case KILLER_1:
    ++stage_;
    if (killer_[0]) return killer_[0];
    return next();
  case KILLER_2:
    ++stage_;
    if (killer_[1]) return killer_[1];
    return next();
  case INIT_QUIET:
    gen_score_quiet();
    ++stage_;
    return next();
  case QUIET:
    if (peek_front()) return pop_front();
    ++stage_;
    return next();
  case BAD_CAP:
    if (peek_back()) return pop_back();
    return NoMove;
  case QSEARCH:
    if (peek_front()) return pop_front();
    return NoMove;
  };

  return NoMove;
}

template class MovePicker<White>;
template class MovePicker<Black>;

/******************************************\
|==========================================|
|             Is Legal function            |
|==========================================|
\******************************************/

template <Colour Us>
bool Board::is_legal(Move move) const {
  using namespace MoveUtils;
  using namespace BBUtils;
  using enum Direction;

  constexpr Direction Up  = Us == White ? N : S;
  constexpr Castle    OO  = Us == White ? WhiteOO : BlackOO;
  constexpr Castle    OOO = Us == White ? WhiteOOO : BlackOOO;

  if (!move) return false;

  // Move params
  const Square    src       = MoveUtils::src(move);
  const Square    dst       = MoveUtils::dst(move);
  const MoveFlag  flag      = MoveUtils::flag(move);
  const Piece     pc        = on(src);
  const PieceType pt        = pt_of(pc);
  const Piece     cap       = on(dst);
  const bool      is_castle = MoveUtils::is_castle(move);
  const bool      is_cap    = MoveUtils::is_capture(move);
  const bool      is_promo  = MoveUtils::is_promo(move);
  const bool      is_ep     = flag == EP;
  const bool      is_dp     = flag == DoublePush;
  const bool      is_quiet  = flag == Quiet;

  // Board info
  const Square ksq_       = ksq<Us>();
  const BB     occ        = bb();
  const BB     hv_pin     = undo_->hv_pin;
  const BB     diag_pin   = undo_->diag_pin;
  const BB     check_mask = undo_->check_mask;
  const BB     attacked   = undo_->attacked;

  // Check if the moving piece is ours or not
  const bool invalid_pc = pc == NoPiece || colour_of(pc) != Us;
  // Check if the captured piece is theirs or not (Except for castling
  // where we can capture our own rook)
  const bool invalid_cap = !is_castle && cap != NoPiece && colour_of(cap) == Us;
  // Check if the squares makes sense (Chess960 allows for src == dst
  // for castling)
  const bool invalid_sq = !is_castle && src == dst;
  // Check if the capture flag is consistent with a piece being
  // captured
  const bool invalid_cap_flag = !is_castle && !is_ep && is_cap == (cap == NoPiece);
  // Check if the promotion flag is consistent
  const bool invalid_promo_flag = is_promo && pt_of(pc) != P;

  if (invalid_pc || invalid_cap || invalid_sq || invalid_cap_flag || invalid_promo_flag)
    return false;

  if (is_castle) {
    if (flag == KingCastle && undo_->c_rights & OO) return !in_check() && can_castle<Us, false>();
    if (flag == QueenCastle && undo_->c_rights & OOO) return !in_check() && can_castle<Us, true>();
    return false;
  }

  if (pt == K) return KING_ATK[src] & from(dst) && !(attacked & from(dst));

  // Check if there is a pin and if so check if the movement is aligned
  // to the king
  const bool valid_pin =
      !(diag_pin & from(src) || hv_pin & from(src)) || is_aligned(src, dst, ksq_);

  if (pt == P) {
    // Enpassant is illegal if the destination is incorrect, the attack
    // pattern is wrong, or the move results in check.
    if (is_ep)
      return undo_->ep == dst && valid_pin && PAWN_ATK[Us][src] & from(dst) & shift<Up>(check_mask);
    // Capture is illegal if the move does not correspond to the attack
    // pattern
    if (is_cap && !(PAWN_ATK[Us][src] & from(dst))) return false;
    // Double Push is illegal if there are pieces in the way
    if (is_dp && (BTWN_BB[src][dst] | from(dst)) & occ) return false;
    // Single Push is illegal if its the wrong pattern and there are
    // pieces in the way
    if (is_quiet && (dst != forward<Us>(src) || from(dst) & occ)) return false;

    return valid_pin && from(dst) & check_mask;
  }

  return valid_pin && from(dst) & check_mask & attack_bb<Us>(pt, src, occ);
}

template bool Board::is_legal<White>(Move move) const;
template bool Board::is_legal<Black>(Move move) const;

/******************************************\
|==========================================|
|               SEE function               |
|==========================================|
\******************************************/

inline BB Board::attackers_to(Square to, BB occ) const {
  return (PAWN_ATK[Black][to] & bb(White, P)) | (PAWN_ATK[White][to] & bb(Black, P))
         | (KNIGHT_ATK[to] & bb(N)) | (BISHOP_ATK[to][occ] & bb(B, Q))
         | (ROOK_ATK[to][occ] & bb(R, Q)) | (KING_ATK[to] & bb(K));
}

bool Board::see(Move move, Eval lower_bound) const {
  using namespace MoveUtils;
  using namespace BBUtils;

  const Square    src       = MoveUtils::src(move);
  const Square    dst       = MoveUtils::dst(move);
  const MoveFlag  flag      = MoveUtils::flag(move);
  const PieceType promo     = MoveUtils::promoted_pt(move);
  const PieceType att       = pt_of(on(src));
  const PieceType vic       = pt_of(on(dst));
  const Square    ep_target = stm_ == White ? shift<Direction::N>(dst) : shift<Direction::S>(dst);
  const BB        diag_sliders = bb(B, Q);
  const BB        hv_sliders   = bb(R, Q);

  // Calculate move value
  Eval gain = -lower_bound;
  if (is_castle(move)) return 0 <= gain;
  if (is_capture(move)) gain += flag == EP ? SeePieceVals[P] : SeePieceVals[vic];
  if (is_promo(move)) gain += SeePieceVals[promo] - SeePieceVals[att];

  // If we are still losing after the capture then, it is a bad move
  if (gain < EvalDraw) return false;
  // Simulate recapture
  gain -= is_promo(move) ? SeePieceVals[promo] : SeePieceVals[att];
  // If we are still winning after recapture then, it is a good move
  if (gain >= EvalDraw) return true;

  // Simulate the capture
  BB occ = (bb() ^ from(src)) | from(dst);
  if (flag == EP) occ ^= from(ep_target);

  // Simulate the rest of the exchanges
  Colour    stm  = ~stm_;
  BB        atks = attackers_to(dst, occ);
  BB        stm_atks, b;
  PieceType lva_pt = NoPieceType;

  // Switch sides in every iteration,
  while (true) {
    // Restrict attackers to occupancy
    atks &= occ;
    // Get attackers from our side
    stm_atks = atks & bb(stm);
    if (!stm_atks) break;

    // Get least valuable attacker and update the gains
    for (const PieceType pt : {P, N, B, R, Q, K}) {
      if (!(b = stm_atks & bb(stm, pt))) continue;
      // Remove attacking piece
      lva_pt = pt;
      occ ^= from(lsb(b));
      break;
    }

    // Register discover attacks
    if (lva_pt == P || lva_pt == B || lva_pt == Q) atks |= BISHOP_ATK[dst][occ] & diag_sliders;
    if (lva_pt == R || lva_pt == Q) atks |= ROOK_ATK[dst][occ] & hv_sliders;

    // Switch sides
    stm = ~stm;
    // Speculate the gains after recapture (discourage drawing
    // captures)
    gain = -SeePieceVals[lva_pt] - gain - 1;

    if (gain >= EvalDraw) {
      // If we capture with the king but the opponent can recapture,
      // then they win
      if (lva_pt == K && atks & bb(stm)) return stm == stm_;
      break;
    }
  }

  return stm != stm_;
}

} // namespace Lyra
