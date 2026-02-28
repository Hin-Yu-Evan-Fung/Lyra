#pragma once

#include "board/board.hpp"
#include "core/defs.hpp"
#include "utils/utils.hpp"

#include <algorithm>
#include <cstdlib>

namespace Lyra {

/******************************************\
|==========================================|
|               Killer Moves               |
|==========================================|
\******************************************/

struct Killer {
  NDArray<Move, 2> moves;

  void clear() { moves.fill(NoMove); }

  void update(Move best) {
    if (best == moves[0]) return;
    moves[1] = moves[0];
    moves[0] = best;
  }
};

/******************************************\
|==========================================|
|              History Entries             |
|==========================================|
\******************************************/

template <Eval Max> struct Hist {
  Eval eval;

  void update(Eval bonus) {
    int clamped = std::clamp(bonus, -Max, Max);
    eval += clamped - eval * std::abs(clamped) / Max;
  }
};

/******************************************\
|==========================================|
|            History Heuristics            |
|==========================================|
\******************************************/

namespace {

using namespace MoveUtils;

constexpr Piece     attacker(const Board &board, Move move) { return board.on(src(move)); }
constexpr PieceType victim(const Board &board, Move move) {
  return is_ep(move) ? P : board.pt_on(dst(move));
}

} // namespace

class MainHist final : public NDArray<Hist<HistMax>, NPiece, NSquare> {
  auto &ref(this auto &self, const Board &b, Move m) { return self[attacker(b, m)][dst(m)]; }

public:
  void clear() { fill({}); }
  Eval get(const Board &b, Move m) const { return ref(b, m).eval; }
  void update(const Board &b, Move m, Eval bon) { ref(b, m).update(bon); }
};

class CapHist final : public NDArray<Hist<HistMax>, NPiece, NSquare, NPieceType> {
  auto &ref(this auto &self, const Board &b, Move m) {
    return self[attacker(b, m)][dst(m)][victim(b, m)];
  }

public:
  void clear() { fill({}); }
  Eval get(const Board &b, Move m) const { return ref(b, m).eval; }
  void update(const Board &b, Move m, Eval bon) { ref(b, m).update(bon); }
};

class ContHist final : public NDArray<Hist<HistMax>, NPiece, NSquare> {
  auto &ref(this auto &self, const Board &b, Move m) { return self[attacker(b, m)][dst(m)]; }

public:
  void clear() { fill({}); }
  Eval get(const Board &b, Move m) const { return ref(b, m).eval; }
  void update(const Board &b, Move m, Eval bon) { ref(b, m).update(bon); }
};

class ContTable final : public NDArray<ContHist, NPiece, NSquare> {
  auto &ref(this auto &self, const Board &b, Move m) { return self[attacker(b, m)][dst(m)]; }

public:
  void clear() {
    for (Piece pc = wP; pc <= bK; ++pc)
      for (Square sq = A1; sq <= H8; ++sq) (*this)[pc][sq].clear();
  }
  ContHist &probe(const Board &b, Move m) { return ref(b, m); }
};

} // namespace Lyra
