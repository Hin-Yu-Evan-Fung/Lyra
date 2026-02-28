#include "engine/uci.hpp"
#include "search/eval.hpp"
#include "utils/bench.hpp"

using namespace Lyra;

int main(int argc, char *argv[]) {
  BBUtils::init();
  Zobrist::init();
  EvalUtils::init();

  if (argc > 1) {
    std::string mode = argv[1];
    if (mode == "bench") run_bench(argc, argv);
  } else {
    UCI uci;
    uci.loop();
  }

  return 0;
}
