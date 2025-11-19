#include "uci.hpp"

#include <ios>
#include <iostream>
#include <sstream>
#include <string>

#include "perft.hpp"

namespace Lyra {

void UCI::loop() {
  std::string input, token;

  engine_.newgame();

  do {
    std::getline(std::cin, input);

    std::istringstream is(input);

    token.clear();
    is >> std::skipws >> token;

    if (token == "uci")
      printf("id name: %s\nid author %s\nversion: %s\nuciok\n", NAME.data(), AUTHOR.data(), VERSION.data());
    else if (token == "isready")
      printf("readyok\n");
    else if (token == "stop" || token == "quit")
      engine_.stop();
    else if (token == "ucinewgame")
      engine_.newgame();
    else if (token == "go")
      parse_go(is);
    else if (token == "position")
      parse_pos(is);
    else if (token == "b")
      engine_.print_pos();
    else if (token == "setoption")
      ;
    else if (token == "perft")
      parse_perft(is);

  } while (token != "quit");
}

void UCI::parse_perft(std::istringstream& is) {
  std::string    token;
  Depth          depth = 0;
  std::streampos pos   = is.tellg();

  if (is >> depth) {
    engine_.perft<Perft>(depth);
    return;
  }

  is.clear();
  is.seekg(pos);
  is >> token;

  if (token == "mp") {
    is >> depth;
    engine_.perft<Perft_MP>(depth);
  } else if (token == "bench")
    engine_.perft_bench();
  else
    printf("Wrong command format! Must be perft [depth], perft mp [depth] or perft bench!\n");
}

void UCI::parse_go(std::istringstream& is) {
  std::string token;

  TimeControl tc{};

  while (is >> token) {
    if (token == "wtime")
      is >> tc.time[White];
    else if (token == "btime")
      is >> tc.time[Black];
    else if (token == "winc")
      is >> tc.inc[White];
    else if (token == "binc")
      is >> tc.inc[Black];
    else if (token == "depth")
      is >> tc.depth;
    else if (token == "movestogo")
      is >> tc.moves_to_go;
    else if (token == "movetime")
      is >> tc.move_time;
    else if (token == "infinite")
      tc.is_infinite = true;
    else {
      printf("Wrong command format!\n");
      return;
    }
  }

  engine_.go(tc);
}

void UCI::parse_pos(std::istringstream& is) {
  std::string token, fen;
  is >> token;

  if (token == "startpos") {
    fen = start_pos.data();
    is >> token;
  } else if (token == "fen") {
    while (is >> token && token != "moves")
      fen += token + " ";
  } else {
    printf(
      "Wrong command format! Must be 'position startpos [moves] <move-1> <move-2> ...' or 'position fen <fen> "
      "[moves] <move-1> <move-2>'\n"
    );
    return;
  }

  std::vector<std::string> moves;

  while (is >> token)
    moves.push_back(token);

  try {
    engine_.set_pos(fen, moves);
  } catch (const std::invalid_argument& e) { printf("Error: %s\n", e.what()); }
}

}  // namespace Lyra
