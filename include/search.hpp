#pragma once

#include "board.hpp"
#include "defs.hpp"
#include "utils.hpp"

namespace Lyra {

class ThreadPool;

struct SearchConfig {
    Time     time[NColour];
    Time     inc[NColour];
    Depth    depth;
    unsigned moves_to_go;
    Time     move_time;
    bool     is_infinite;
};

struct Worker {
    enum NodeType { Root, PV, NonPV };

public:
    Worker(ThreadPool& tp, size_t id) : threads_(tp), id_(id) {}

    void reset();
    void start();

    constexpr bool is_main() const { return id_ == 0; }

private:
    void iter_deep();
    void asp_win();
    template <NodeType NT>
    void search(Board& board);
    template <NodeType NT>
    void qsearch(Board& board);
    void eval(const Board& board);

    ThreadPool& threads_;
    size_t      id_;

    Board board_;
};

}  // namespace Lyra
