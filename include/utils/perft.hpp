#pragma once

#include "board/board.hpp"
#include "board/movegen.hpp"
#include "core/defs.hpp"
#include "search/history.hpp"
#include "search/movepick.hpp"

#include <print>

namespace Lyra {

enum PerftMode {
  Perft,
  Perft_MP,
};

namespace {
Killer killer;
MainHist ht;
CapHist cap_ht;
} // namespace

template <bool Div, Colour Us> U64 perft(Board &board, Depth depth) {
  U64 total = 0;

  if (!Div && depth <= 1) {
    enum_moves<Us, GenAll>(board, [&](Move move) { ++total; });
    return total;
  }

  enum_moves<Us, GenAll>(board, [&](Move move) {
    U64 n = 0;

    if (depth == 1)
      n = Div ? 1 : 0;
    else {
      board.do_move<Us>(move);
      n = perft<false, ~Us>(board, depth - 1);
      board.undo_move<Us>();
    }

    total += n;

    if (Div && n > 0) std::println("       {}: {}", MoveUtils::format(move, board.chess960), n);
  });

  return total;
}

template <bool Div, Colour Us> U64 perftmp(Board &board, Depth depth) {
  MovePicker<Us> mp(board, killer, ht, cap_ht, NoMove, 0);

  if (!Div && depth <= 1) {
    U64 n = 0;
    while (mp.next()) n++;
    return n;
  }

  Move move;
  U64 total = 0;

  while ((move = mp.next())) {
    U64 n = 0;

    if (depth == 1) {
      return Div ? 1 : 0;
    } else {
      board.do_move<Us>(move);
      n = perftmp<false, ~Us>(board, depth - 1);
      board.undo_move<Us>();
    }

    total += n;

    if (Div && n > 0) std::println("       {}: {}", MoveUtils::format(move, board.chess960), n);
  }

  return total;
}

template <PerftMode PM> void perft(Board &board, Depth depth) {
  std::println("========   PERFT   ========");

  Time start = now();
  U64 nodes = 0;

  if constexpr (PM == Perft_MP)
    nodes = board.stm() == White ? perftmp<true, White>(board, depth) : perftmp<true, Black>(board, depth);
  else
    nodes = board.stm() == White ? perft<true, White>(board, depth) : perft<true, Black>(board, depth);

  Time elapsed = now() - start;

  int nps = 0;
  if (elapsed > 0) {
    nps = nodes * 1000 / elapsed;
  }

  std::println("\n========  RESULTS  ========");
  std::println("     use_mp: {}              ", PM == Perft_MP ? "True" : "False");
  std::println("      nodes: {}             ", nodes);
  std::println("       time: {} ms          ", elapsed);
  std::println("        nps: {:.1f} Mnps       ", (float)nps / 1e6);
  std::println("===========================  ");
}

void perft_bench();

} // namespace Lyra
