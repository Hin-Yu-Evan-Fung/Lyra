#include "engine.hpp"

#include "defs.hpp"
#include "movegen.hpp"
#include "perft.hpp"

namespace Lyra {

Engine::Engine() : pool_(THREADS) {}

void Engine::wait_for_search_finish() {}
void Engine::set_pos(const std::string fen, const std::vector<std::string>& moves) {
    board_.set(fen);

    for (std::string move_str : moves) {
        Move parsed = NoMove;

        for (Move move : list_moves(board_)) {
            if (to_str(move) == move_str) {
                parsed = move;
                break;
            }
        }

        if (parsed != NoMove)
            board_.do_move(parsed);
        else
            throw std::invalid_argument(std::format("Move %s is illegal!", move_str.data()));
    }
}

void Engine::print_pos() { board_.print(); }

void Engine::perft(Depth d) { Lyra::perft(board_, d); }
void Engine::newgame() { board_.set(start_pos.data()); }
void Engine::go(SearchConfig sc) {}
void Engine::stop() {}

}  // namespace Lyra
