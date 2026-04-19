#include "eval.hpp"
#include "params.hpp"
#include "uci.hpp"

using namespace Lyra;

int main(int argc, char *argv[]) {
  BBUtils::init();
  Zobrist::init();
  EvalUtils::init();

  if (argc > 1) {
    std::string mode = argv[1];
    if (mode == "bench") run_bench(argc, argv);
#ifdef TUNE
    if (mode == "spsa") TunableRegistry::instance().print_spsa_params();
#endif
  } else {
    UCI uci;
    uci.loop();
  }

  return 0;
}
