#include "uci.hpp"

#include "perft.hpp"

#include <ios>
#include <iostream>
#include <print>
#include <sstream>
#include <string>

namespace Lyra {

void UCI::loop() {
  std::string input, token;

  engine_.newgame();

  do {
    std::getline(std::cin, input);

    std::istringstream is(input);

    token.clear();
    is >> std::skipws >> token;

    if (token == "uci") {
      std::println("id name {} {}", NAME, VERSION);
      std::println("id author {}", AUTHOR);
      std::println("option name UCI_Chess960 type check default false");
      std::println("option name ClearHash type button");
      std::println("option name Hash type spin default 32 min 1 max 128");
      std::println("option name Threads type spin default 1 min 1 max 12");
#ifdef TUNE
      TunableRegistry::instance().print_options();
#endif
      std::println("uciok");
    } else if (token == "isready")
      std::println("readyok");
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
      parse_option(is);
    else if (token == "perft")
      parse_perft(is);

  } while (token != "quit");
}

void UCI::parse_perft(std::istringstream &is) {
  std::string    token;
  Depth          depth = 0;
  std::streampos pos   = is.tellg();

  if (is >> depth) {
    engine_.perft(PerftMode::Normal, depth);
    return;
  }

  is.clear();
  is.seekg(pos);
  is >> token;

  if (token == "mp") {
    is >> depth;
    engine_.perft(PerftMode::MovePick, depth);
  } else if (token == "bench")
    engine_.perft_bench();
  else
    std::println("Wrong command format! Must be perft [depth], perft mp [depth] or perft bench!");
}

void UCI::parse_go(std::istringstream &is) {
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
      std::println("Wrong command format!");
      return;
    }
  }

  engine_.go(tc);
}

void UCI::parse_pos(std::istringstream &is) {
  std::string token, fen;
  is >> token;

  if (token == "startpos") {
    fen = start_pos.data();
    is >> token;
  } else if (token == "fen") {
    while (is >> token && token != "moves") fen += token + " ";
  } else {
    std::println("Wrong command format! Must be 'position startpos [moves] <move-1> <move-2> ...' "
                 "or 'position fen <fen> "
                 "[moves] <move-1> <move-2>'");
    return;
  }

  std::vector<std::string> moves;

  while (is >> token) moves.push_back(token);

  try {
    engine_.set_pos(fen, moves);
  } catch (const std::invalid_argument &e) {
    std::println("Error: {}", e.what());
  }
}

void UCI::parse_option(std::istringstream &is) {
  std::string token, name, value;
  is >> token;

  if (token != "name") return;

  while (is >> token && token != "value") name += (name.empty() ? "" : " ") + token;

  while (is >> token) value += (value.empty() ? "" : " ") + token;

#ifdef TUNE
  TunableRegistry::instance().set(name, std::stoi(value));
#endif

  if (name == "UCI_Chess960") {
    engine_.set_chess960(value == "true");
  } else if (name == "Clear Hash") {
    engine_.clear_tt();
  } else if (name == "Threads") {
    size_t n = std::stoi(value);
    if (n >= 1 && n <= 32) engine_.set_threads(n);
  } else if (name == "Hash") {
    size_t mb = std::stoi(value);
    if (mb >= 1 && mb <= 128) engine_.set_tt_size(mb);
  }
}

} // namespace Lyra
