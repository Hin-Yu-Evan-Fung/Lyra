#include "perft.hpp"

#include "movegen.hpp"
#include "movepick.hpp"
#include "search.hpp"

#include <print>

namespace Lyra {

constexpr Depth    BenchDepth = 7;
constexpr unsigned NBenchPos  = 66;

/******************************************\
|==========================================|
|               Perft Tests                |
|==========================================|
\******************************************/

template <bool Div, Colour Us>
U64 perft(Board &board, Depth depth) {
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

namespace {
Killer    killer{};
HistQuiet hist_quiet{};
} // namespace

template <bool Div, Colour Us>
U64 perftmp(Board &board, Depth depth) {
  MovePicker<Us> mp(MPType::Main, board, {&killer, &hist_quiet}, NoMove, 0);

  if (!Div && depth <= 1) {
    U64 n = 0;
    while (mp.next()) n++;
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

void perft(PerftMode perft_mode, Board &board, Depth depth) {
  std::println("========   PERFT   ========");

  Time start = now();
  U64  nodes = 0;

  if (perft_mode == PerftMode::MovePick)
    nodes = board.stm() == White ? perftmp<true, White>(board, depth)
                                 : perftmp<true, Black>(board, depth);
  else
    nodes =
        board.stm() == White ? perft<true, White>(board, depth) : perft<true, Black>(board, depth);

  Time elapsed = now() - start;

  int nps = 0;
  if (elapsed > 0) {
    nps = nodes * 1000 / elapsed;
  }

  std::println("\n========  RESULTS  ========");
  std::println("     use_mp: {}              ",
               perft_mode == PerftMode::MovePick ? "True" : "False");
  std::println("      nodes: {}             ", nodes);
  std::println("       time: {} ms          ", elapsed);
  std::println("        nps: {:.1f} Mnps       ", (float)nps / 1e6);
  std::println("===========================  ");
}

/******************************************\
|==========================================|
|               Perft Bench                |
|==========================================|
\******************************************/

// clang-format off
BenchTestCase default_cases[] = {
    {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 6, 119060324},
    {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 5, 193690690},
    {"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 7, 178633661},
    {"r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 6, 706045033},
    {"1k6/1b6/8/8/7R/8/8/4K2R b K - 0 1", 5, 1063513},
    {"3k4/3p4/8/K1P4r/8/8/8/8 b - - 0 1", 6, 1134888},
    {"8/8/4k3/8/2p5/8/B2P2K1/8 w - - 0 1", 6, 1015133},
    {"8/8/1k6/2b5/2pP4/8/5K2/8 b - d3 0 1", 6, 1440467},
    {"5k2/8/8/8/8/8/8/4K2R w K - 0 1", 6, 661072},
    {"3k4/8/8/8/8/8/8/R3K3 w Q - 0 1", 6, 803711},
    {"r3k2r/1b4bq/8/8/8/8/7B/R3K2R w KQkq - 0 1", 4, 1274206},
    {"r3k2r/8/3Q4/8/8/5q2/8/R3K2R b KQkq - 0 1", 4, 1720476},
    {"2K2r2/4P3/8/8/8/8/8/3k4 w - - 0 1", 6, 3821001},
    {"8/8/1P2K3/8/2n5/1q6/8/5k2 b - - 0 1", 5, 1004658},
    {"4k3/1P6/8/8/8/8/K7/8 w - - 0 1", 6, 217342},
    {"8/P1k5/K7/8/8/8/8/8 w - - 0 1", 6, 92683},
    {"K1k5/8/P7/8/8/8/8/8 w - - 0 1", 6, 2217},
    {"8/k1P5/8/1K6/8/8/8/8 w - - 0 1", 7, 567584},
    {"8/8/2k5/5q2/5n2/8/5K2/8 b - - 0 1", 4, 23527},
    {"4k3/8/8/8/8/8/8/4K2R w K - 0 1 ", 6, 764643},
    {"4k3/8/8/8/8/8/8/R3K3 w Q - 0 1 ", 6, 846648},
    {"4k2r/8/8/8/8/8/8/4K3 w k - 0 1 ", 6, 899442},
    {"r3k3/8/8/8/8/8/8/4K3 w q - 0 1 ", 6, 1001523},
    {"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 5, 674624},
    {"r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 5, 15833292},
    {"rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8  ", 5, 89941194},
    {"r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",5, 164075551},
    {"2K2r2/4P3/8/8/8/8/8/3k4 w - - 0 1", 6, 3821001},
    {"K1k5/8/P7/8/8/8/8/8 w - - 0 1", 6, 2217},
    {"8/8/1k6/2b5/2pP4/8/5K2/8 b - d3 0 1", 6, 1440467},
    {"3k4/3p4/8/K1P4r/8/8/8/8 b - - 0 1", 6, 1134888},
    {"rbbknnqr/pppppppp/8/8/8/8/PPPPPPPP/RBBKNNQR w KQkq - 0 1", 6, 124381396},
    {"bnrkrnqb/pppppppp/8/8/8/8/PPPPPPPP/BNRKRNQB w KQkq - 0 1", 6, 146858295},
    {"nrbbqknr/pppppppp/8/8/8/8/PPPPPPPP/NRBBQKNR w KQkq - 0 1", 6, 97939069},
    {"bnrbnkrq/pppppppp/8/8/8/8/PPPPPPPP/BNRBNKRQ w KQkq - 0 1", 6, 145999259},
    {"rbknqnbr/pppppppp/8/8/8/8/PPPPPPPP/RBKNQNBR w KQkq - 0 1", 6, 126480040},
    {"qbrnnkbr/pppppppp/8/8/8/8/PPPPPPPP/QBRNNKBR w KQkq - 0 1", 6, 121613156},
    {"8/3k4/8/8/8/8/8/rR2K3 w Q - 0 1", 6, 7137508},
    {"Rr2k3/8/8/8/8/8/8/rR2K3 w Qq - 0 1", 6, 46081241},
    {"2k5/8/8/8/b7/8/8/2K3R1 w - - 0 1", 6, 6578528},
    {"3k4/8/8/8/8/8/8/rRK5 w - - 0 1", 6, 3191684},
    {"1rkr4/8/8/8/8/8/8/1RKR4 w KQkq - 0 1", 6, 66969143},
    {"3k4/3q1q2/8/8/8/4Q3/3P4/1R1K2R1 w KQ - 0 1", 6, 2938241633},
    {"1b1qbkrn/1prp1pp1/pn5p/2p1pB2/Q1PP4/1N6/PP2PPPP/2R1BKRN w KQk - 2 9", 6, 1648762553},
    {"1rkb1qr1/pppp2pp/1n2p1n1/3b1p2/3N3P/P2P1P2/1PP1P1P1/1RKBBQRN w KQkq - 3 9",6, 1042669941},
    {"1b1r1krb/ppp1np2/qn1p2pp/3Bp3/2P1P1PP/1N1P4/PP3P2/1BNRQKR1 w KQkq - 0 9", 6, 1169912833}
};
// clang-format on

bool perft_bench() { return perft_bench(PerftMode::Normal, default_cases, 46); }

bool perft_bench(PerftMode perft_mode, BenchTestCase test_cases[], int n_cases) {
  Board board;
  U64   nodes = 0;

  for (int i = 0; i < n_cases; i++) {
    auto &[fen, depth, validation] = test_cases[i];

    board.set(fen);

    Time start = now();

    if (perft_mode == PerftMode::MovePick)
      nodes = board.stm() == White ? perftmp<false, White>(board, depth)
                                   : perftmp<false, Black>(board, depth);
    else
      nodes = board.stm() == White ? perft<false, White>(board, depth)
                                   : perft<false, Black>(board, depth);

    Time elapsed = now() - start;

    int nps = 0;
    if (elapsed > 0) {
      nps = nodes * 1000 / elapsed;
    }

    std::println("Status: {}, Time: {} ms, NPS: {}, Fen: {}",
                 (nodes == validation) ? "PASSED" : "FAILED", elapsed, nps, board.fen());

    if (nodes != validation) return false;
  }
  return true;
}

/******************************************\
|==========================================|
|             OpenBench bench              |
|==========================================|
\******************************************/

constexpr std::string_view bench_fens[NBenchPos] = {
    "1r2r2k/1b4q1/pp5p/2pPp1p1/P3Pn2/1P1B1Q1P/2R3P1/4BR1K b - - 1 37",
    "1r4k1/4ppb1/2n1b1qp/pB4p1/1n1BP1P1/7P/2PNQPK1/3RN3 w - - 8 29",
    "1r5k/2pq2p1/3p3p/p1pP4/4QP2/PP1R3P/6PK/8 w - - 1 51",
    "1rb1rn1k/p3q1bp/2p3p1/2p1p3/2P1P2N/PP1RQNP1/1B3P2/4R1K1 b - - 4 23",
    "2q3r1/1r2pk2/pp3pp1/2pP3p/P1Pb1BbP/1P4Q1/R3NPP1/4R1K1 w - - 2 34",
    "2r2b2/5p2/5k2/p1r1pP2/P2pB3/1P3P2/K1P3R1/7R w - - 23 93",
    "2r2k2/8/4P1R1/1p6/8/P4K1N/7b/2B5 b - - 0 55",
    "2r4r/1p4k1/1Pnp4/3Qb1pq/8/4BpPp/5P2/2RR1BK1 w - - 0 42",
    "2rqr1k1/1p3p1p/p2p2p1/P1nPb3/2B1P3/5P2/1PQ2NPP/R1R4K w - - 3 25",
    "2rr2k1/1p4bp/p1q1p1p1/4Pp1n/2PB4/1PN3P1/P3Q2P/2RR2K1 w - f6 0 20",
    "3br1k1/p1pn3p/1p3n2/5pNq/2P1p3/1PN3PP/P2Q1PB1/4R1K1 w - - 0 23",
    "3q1k2/3P1rb1/p6r/1p2Rp2/1P5p/P1N2pP1/5B1P/3QRK2 w - - 1 42",
    "3qk1b1/1p4r1/1n4r1/2P1b2B/p3N2p/P2Q3P/8/1R3R1K w - - 2 39",
    "3qr2k/1p3rbp/2p3p1/p7/P2pBNn1/1P3n2/6P1/B1Q1RR1K b - - 1 30",
    "3r1rk1/1pp1pn1p/p1n1q1p1/3p4/Q3P3/2P5/PP1NBPPP/4RRK1 w - - 0 12",
    "3r3k/2r4p/1p1b3q/p4P2/P2Pp3/1B2P3/3BQ1RP/6K1 w - - 3 87",
    "3r4/ppq1ppkp/4bnp1/2pN4/2P1P3/1P4P1/PQ3PBP/R4K2 b - - 2 20",
    "4q1bk/6b1/7p/p1p4p/PNPpP2P/KN4P1/3Q4/4R3 b - - 0 37",
    "4q1bk/6b1/7p/p1p4p/PNPpP2P/KN4P1/3Q4/4R3 b - - 0 37",
    "4r1k1/1q1r3p/2bPNb2/1p1R3Q/pB3p2/n5P1/6B1/4R1K1 w - - 2 36",
    "4r1k1/4r1p1/8/p2R1P1K/5P1P/1QP3q1/1P6/3R4 b - - 0 1",
    "4r2k/1p3rbp/2p1N1p1/p3n3/P2NB1nq/1P6/4R1P1/B1Q2RK1 b - - 4 32",
    "4rrk1/2p1b1p1/p1p3q1/4p3/2P2n1p/1P1NR2P/PB3PP1/3R1QK1 b - - 2 24",
    "4rrk1/pp1n1pp1/q5p1/P1pP4/2n3P1/7P/1P3PB1/R1BQ1RK1 w - - 3 22",
    "5k2/4q1p1/3P1pQb/1p1B4/pP5p/P1PR4/5PP1/1K6 b - - 0 38",
    "5rk1/1pp1pn1p/p3Brp1/8/1n6/5N2/PP3PPP/2R2RK1 w - - 2 20",
    "5rk1/1rP3pp/p4n2/3Pp3/1P2Pq2/2Q4P/P5P1/R3R1K1 b - - 0 32",
    "5rr1/4n2k/4q2P/P1P2n2/3B1p2/4pP2/2N1P3/1RR1K2Q w - - 1 49",
    "6k1/1R3p2/6p1/2Bp3p/3P2q1/P7/1P2rQ1K/5R2 b - - 4 44",
    "6k1/5pp1/8/2bKP2P/2P5/p4PNb/B7/8 b - - 1 44",
    "6k1/6p1/8/6KQ/1r6/q2b4/8/8 w - - 0 32",
    "6r1/5k2/p1b1r2p/1pB1p1p1/1Pp3PP/2P1R1K1/2P2P2/3R4 w - - 1 36",
    "7r/2p3k1/1p1p1qp1/1P1Bp3/p1P2r1P/P7/4R3/Q4RK1 w - - 0 36",
    "8/1R6/1p1K1kp1/p6p/P1p2P1P/6P1/1Pn5/8 w - - 0 67",
    "8/1p2pk1p/p1p1r1p1/3n4/8/5R2/PP3PPP/4R1K1 b - - 3 27",
    "8/4pk2/1p1r2p1/p1p4p/Pn5P/3R4/1P3PP1/4RK2 w - - 1 33",
    "8/5k2/1p4p1/p1pK3p/P2n1P1P/6P1/1P6/4R3 b - - 14 63",
    "8/5k2/1pnrp1p1/p1p4p/P6P/4R1PK/1P3P2/4R3 b - - 1 38",
    "8/6pk/2b1Rp2/3r4/1R1B2PP/P5K1/8/2r5 b - - 16 42",
    "8/8/1p1k2p1/p1prp2p/P2n3P/6P1/1P1R1PK1/4R3 b - - 5 49",
    "8/8/1p1kp1p1/p1pr1n1p/P6P/1R4P1/1P3PK1/1R6 b - - 15 45",
    "8/8/1p2k1p1/3p3p/1p1P1P1P/1P2PK2/8/8 w - - 3 54",
    "8/8/1p4p1/p1p2k1p/P2n1P1P/4K1P1/1P6/6R1 b - - 6 59",
    "8/8/1p4p1/p1p2k1p/P2n1P1P/4K1P1/1P6/6R1 b - - 6 59",
    "8/8/1p4p1/p1p2k1p/P2npP1P/4K1P1/1P6/3R4 w - - 6 54",
    "8/8/1p4p1/p1p2k1p/P2npP1P/4K1P1/1P6/3R4 w - - 6 54",
    "8/p2B4/PkP5/4p1pK/4Pb1p/5P2/8/8 w - - 29 68",
    "R4r2/4q1k1/2p1bb1p/2n2B1Q/1N2pP2/1r2P3/1P5P/2B2KNR w - - 3 31",
    "q5k1/5ppp/1r3bn1/1B6/P1N2P2/BQ2P1P1/5K1P/8 b - - 2 34",
    "r1b2k1r/5n2/p4q2/1ppn1Pp1/3pp1p1/NP2P3/P1PPBK2/1RQN2R1 w - - 0 22",
    "r1b2k1r/5n2/p4q2/1ppn1Pp1/3pp1p1/NP2P3/P1PPBK2/1RQN2R1 w - - 0 22",
    "r1b2rk1/p1q1ppbp/6p1/2Q5/8/4BP2/PPP3PP/2KR1B1R b - - 2 14",
    "r1bq1rk1/pp2b1pp/n1pp1n2/3P1p2/2P1p3/2N1P2N/PP2BPPP/R1BQ1RK1 b - - 2 10",
    "r1bq2k1/p4r1p/1pp2pp1/3p4/1P1B3Q/P2B1N2/2P3PP/4R1K1 b - - 2 19",
    "r1bqk2r/pppp1ppp/5n2/4b3/4P3/P1N5/1PP2PPP/R1BQKB1R w KQkq - 0 5",
    "r1bqr1k1/pp1p1ppp/2p5/8/3N1Q2/P2BB3/1PP2PPP/R3K2n b Q - 1 12",
    "r2qr1k1/pb1nbppp/1pn1p3/2ppP3/3P4/2PB1NN1/PP3PPP/R1BQR1K1 w - - 4 12",
    "r3k2r/2pb1ppp/2pp1q2/p7/1nP1B3/1P2P3/P2N1PPP/R2QK2R w KQkq a6 0 14",
    "r3k2r/ppp1pp1p/2nqb1pn/3p4/4P3/2PP4/PP1NBPPP/R2QK1NR w KQkq - 1 5",
    "r3kbbr/pp1n1p1P/3ppnp1/q5N1/1P1pP3/P1N1B3/2P1QP2/R3KB1R b KQkq b3 0 17",
    "r3qbrk/6p1/2b2pPp/p3pP1Q/PpPpP2P/3P1B2/2PB3K/R5R1 w - - 16 42",
    "r4qk1/6r1/1p4p1/2ppBbN1/1p5Q/P7/2P3PP/5RK1 w - - 2 25",
    "r6k/pbR5/1p2qn1p/P2pPr2/4n2Q/1P2RN1P/5PBK/8 w - - 2 31",
    "r7/6k1/1p6/2pp1p2/7Q/8/p1P2K1P/8 w - - 0 32",
    "rn2k3/4r1b1/pp1p1n2/1P1q1p1p/3P4/P3P1RP/1BQN1PR1/1K6 w - - 6 28",
    "rnbqkb1r/pppppppp/5n2/8/2PP4/8/PP2PPPP/RNBQKBNR b KQkq c3 0 2",
};

void run_bench(int argc, char **argv) {
  std::atomic_bool stop;

  Board       board;
  TT          tt{TT_SIZE};
  Worker      worker{stop, 0, tt};
  TimeControl tc;

  tc.depth = argc > 2 ? atoi(argv[2]) : BenchDepth;

  U64  total_nodes = 0;
  U64  total_nps   = 0;
  Time total_time  = 0;

  for (unsigned i = 0; i < NBenchPos; ++i) {
    // Reset worker and board
    stop.store(false, std::memory_order_relaxed);
    board.set(bench_fens[i].data());
    worker.reset(board);

    worker.start(tc);

    total_nodes += worker.nodes();
    total_time += worker.clock().elapsed();
    total_nps += worker.nodes() * 1000 / std::max(worker.clock().elapsed(), 1UL);
  }
  std::println("{} nodes {} nps {} time", total_nodes, total_nps / NBenchPos, total_time);
}

}; // namespace Lyra
