#include "engine/uci.hpp"
#include "search/eval.hpp"

using namespace Lyra;

int main() {
  BBUtils::init();
  Zobrist::init();
  EvalUtils::init();

  UCI uci;

  uci.loop();

  return 0;
}
