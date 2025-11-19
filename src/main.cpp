#include "eval.hpp"
#include "uci.hpp"

using namespace Lyra;

int main() {
  BBUtils::init();
  Zobrist::init();
  EvalUtils::init();

  UCI uci;

  uci.loop();

  return 0;
}
