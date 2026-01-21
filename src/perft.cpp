#include "perft.hpp"

#include <print>

#include "defs.hpp"
#include "movegen.hpp"
#include "movepick.hpp"
#include "search.hpp"

namespace Lyra {

namespace {
MainHistory history;
Killer      killer;
}  // namespace

template <bool Div, Colour Us>
U64 perft(Board& board, Depth depth) {
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

template <bool Div, Colour Us>
U64 perftmp(Board& board, Depth depth) {
  MovePicker<Us> mp(board, &killer, &history, NoMove, 0);

  if (!Div && depth <= 1) {
    U64 n = 0;
    while (mp.next())
      n++;
    return n;
  }

  Move move;
  U64  total = 0;

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

template <PerftMode PM>
void perft(Board& board, Depth d) {
  std::println("========   PERFT   ========");

  Time start = now();
  U64  nodes = 0;

  if constexpr (PM == Perft_MP)
    nodes = board.stm() == White ? perftmp<true, White>(board, d) : perftmp<true, Black>(board, d);
  else
    nodes = board.stm() == White ? perft<true, White>(board, d) : perft<true, Black>(board, d);

  Time elapsed = now() - start;

  int nps      = 0;
  if (elapsed > 0) { nps = nodes * 1000 / elapsed; }

  std::println("\n========  RESULTS  ========");
  std::println("     use_mp: {}              ", PM == Perft_MP ? "True" : "False");
  std::println("      nodes: {}             ", nodes);
  std::println("       time: {} ms          ", elapsed);
  std::println("        nps: {:.1f} Mnps       ", (float)nps / 1e6);
  std::println("===========================  ");
}

template void perft<Perft>(Board& board, Depth d);
template void perft<Perft_MP>(Board& board, Depth d);

struct BenchTestCase {
  std::string fen;
  Depth       depth;
  U64         nodes;
};

BenchTestCase tests[] = {
  {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",                  6, 119060324 },
  {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",      5, 193690690 },
  {"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",                                 7, 178633661 },
  {"r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",          6, 706045033 },
  {"1k6/1b6/8/8/7R/8/8/4K2R b K - 0 1",                                         5, 1063513   },
  {"3k4/3p4/8/K1P4r/8/8/8/8 b - - 0 1",                                         6, 1134888   },
  {"8/8/4k3/8/2p5/8/B2P2K1/8 w - - 0 1",                                        6, 1015133   },
  {"8/8/1k6/2b5/2pP4/8/5K2/8 b - d3 0 1",                                       6, 1440467   },
  {"5k2/8/8/8/8/8/8/4K2R w K - 0 1",                                            6, 661072    },
  {"3k4/8/8/8/8/8/8/R3K3 w Q - 0 1",                                            6, 803711    },
  {"r3k2r/1b4bq/8/8/8/8/7B/R3K2R w KQkq - 0 1",                                 4, 1274206   },
  {"r3k2r/8/3Q4/8/8/5q2/8/R3K2R b KQkq - 0 1",                                  4, 1720476   },
  {"2K2r2/4P3/8/8/8/8/8/3k4 w - - 0 1",                                         6, 3821001   },
  {"8/8/1P2K3/8/2n5/1q6/8/5k2 b - - 0 1",                                       5, 1004658   },
  {"4k3/1P6/8/8/8/8/K7/8 w - - 0 1",                                            6, 217342    },
  {"8/P1k5/K7/8/8/8/8/8 w - - 0 1",                                             6, 92683     },
  {"K1k5/8/P7/8/8/8/8/8 w - - 0 1",                                             6, 2217      },
  {"8/k1P5/8/1K6/8/8/8/8 w - - 0 1",                                            7, 567584    },
  {"8/8/2k5/5q2/5n2/8/5K2/8 b - - 0 1",                                         4, 23527     },
  {"4k3/8/8/8/8/8/8/4K2R w K - 0 1 ",                                           6, 764643    },
  {"4k3/8/8/8/8/8/8/R3K3 w Q - 0 1 ",                                           6, 846648    },
  {"4k2r/8/8/8/8/8/8/4K3 w k - 0 1 ",                                           6, 899442    },
  {"r3k3/8/8/8/8/8/8/4K3 w q - 0 1 ",                                           6, 1001523   },
  {"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",                                 5, 674624    },
  {"r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",          5, 15833292  },
  {"rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8  ",               5, 89941194  },
  {"r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10 ", 5, 164075551 },
  {"2K2r2/4P3/8/8/8/8/8/3k4 w - - 0 1",                                         6, 3821001   },
  {"K1k5/8/P7/8/8/8/8/8 w - - 0 1",                                             6, 2217      },
  {"8/8/1k6/2b5/2pP4/8/5K2/8 b - d3 0 1",                                       6, 1440467   },
  {"3k4/3p4/8/K1P4r/8/8/8/8 b - - 0 1",                                         6, 1134888   },
  {"rbbknnqr/pppppppp/8/8/8/8/PPPPPPPP/RBBKNNQR w KQkq - 0 1",                  6, 124381396 },
  {"bnrkrnqb/pppppppp/8/8/8/8/PPPPPPPP/BNRKRNQB w KQkq - 0 1",                  6, 146858295 },
  {"nrbbqknr/pppppppp/8/8/8/8/PPPPPPPP/NRBBQKNR w KQkq - 0 1",                  6, 97939069  },
  {"bnrbnkrq/pppppppp/8/8/8/8/PPPPPPPP/BNRBNKRQ w KQkq - 0 1",                  6, 145999259 },
  {"rbknqnbr/pppppppp/8/8/8/8/PPPPPPPP/RBKNQNBR w KQkq - 0 1",                  6, 126480040 },
  {"qbrnnkbr/pppppppp/8/8/8/8/PPPPPPPP/QBRNNKBR w KQkq - 0 1",                  6, 121613156 },
  {"8/3k4/8/8/8/8/8/rR2K3 w Q - 0 1",                                           6, 7137508   },
  {"Rr2k3/8/8/8/8/8/8/rR2K3 w Qq - 0 1",                                        6, 46081241  },
  {"2k5/8/8/8/b7/8/8/2K3R1 w - - 0 1",                                          6, 6578528   },
  {"3k4/8/8/8/8/8/8/rRK5 w - - 0 1",                                            6, 3191684   },
  {"1rkr4/8/8/8/8/8/8/1RKR4 w KQkq - 0 1",                                      6, 66969143  },
  {"3k4/3q1q2/8/8/8/4Q3/3P4/1R1K2R1 w KQ - 0 1",                                6, 2938241633},
  {"1b1qbkrn/1prp1pp1/pn5p/2p1pB2/Q1PP4/1N6/PP2PPPP/2R1BKRN w KQk - 2 9",       6, 1648762553},
  {"1rkb1qr1/pppp2pp/1n2p1n1/3b1p2/3N3P/P2P1P2/1PP1P1P1/1RKBBQRN w KQkq - 3 9", 6, 1042669941},
  {"1b1r1krb/ppp1np2/qn1p2pp/3Bp3/2P1P1PP/1N1P4/PP3P2/1BNRQKR1 w KQkq - 0 9",   6, 1169912833}
};

void perft_bench() {
  Board board;
  U64   nodes;
  std::println(
    "==================================================  START BENCH  "
    "=================================================="
  );
  for (auto& [fen, depth, validation] : tests) {
    board.set(fen);

    Time start   = now();

    nodes        = board.stm() == White ? perft<false, White>(board, depth) : perft<false, Black>(board, depth);

    Time elapsed = now() - start;

    int nps      = 0;
    if (elapsed > 0) { nps = nodes * 1000 / elapsed; }

    std::println(
      "status: {}, time: {} ms, nps: {}, fen: {}",
      nodes == validation ? "PASSED" : std::format("FAILED ({} != {})", nodes, validation),
      elapsed,
      nps,
      fen
    );

    if (nodes != validation) return;
  }
  std::println(
    "==================================================  ALL PASSED  "
    "=================================================="
  );
}
}  // namespace Lyra
