#include "uci.hpp"

#include <iostream>
#include <sstream>
#include <string>

#include "perft.hpp"

namespace Lyra {

UCI::UCI() {
    BBUtils::init();
    Zobrist::init();
}

void UCI::loop() {
    std::string input, token;

    engine.newgame();

    do {
        std::getline(std::cin, input);

        std::istringstream is(input);

        token.clear();
        is >> std::skipws >> token;

        if (token == "uci")
            printf("id name: %s\nid author %s\nversion: %s\nuciok\n", NAME.data(), AUTHOR.data(), VERSION.data());
        else if (token == "isready")
            printf("readyokay\n");
        else if (token == "quit")
            engine.stop();
        else if (token == "ucinewgame")
            engine.newgame();
        else if (token == "go")
            parse_go(is);
        else if (token == "position")
            parse_pos(is);
        else if (token == "b")
            engine.print_pos();
        else if (token == "bench")
            perft_bench();
        else if (token == "setoption")
            ;

    } while (token != "quit");
}

void UCI::parse_go(std::istringstream& is) {
    std::string token;

    SearchConfig sc;
    bool         is_perft = false;

    while (is >> token) {
        if (token == "wtime")
            is >> sc.time[White];
        else if (token == "btime")
            is >> sc.time[Black];
        else if (token == "winc")
            is >> sc.inc[White];
        else if (token == "binc")
            is >> sc.inc[Black];
        else if (token == "depth")
            is >> sc.depth;
        else if (token == "movestogo")
            is >> sc.moves_to_go;
        else if (token == "movetime")
            is >> sc.move_time;
        else if (token == "infinite")
            sc.is_infinite = true;
        if (token == "perft") {
            is_perft = true;
            is >> sc.depth;
        }
    }

    is_perft ? engine.perft(sc.depth) : engine.go(sc);
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
        engine.set_pos(fen, moves);
    } catch (const std::invalid_argument& e) { printf("Error: %s\n", e.what()); }
}

}  // namespace Lyra
