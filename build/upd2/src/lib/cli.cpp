#include "cli.h"

namespace upd {
namespace cli {

options parse_options(int argc, const char* const argv[]) {
  options result;
  for (++argv, --argc; argc > 0; ++argv, --argc) {
    const auto arg = std::string(*argv);
    if (arg == "--root") {
      result.root = true;
    } else if (arg == "--version") {
      result.version = true;
    } else if (arg == "--help") {
      result.help = true;
    } else if (arg == "--dev") {
      result.dev = true;
    } else {
      throw option_parse_error(arg);
    }
  }
  return result;
}

void print_help() {
  std::cout
    << "usage: upd [options] [targets]" << std::endl << std::endl
    << "Options:" << std::endl
    << "  --version     Print semantic version and exit" << std::endl
    << "  --help        Print usage help and exit" << std::endl
    << "  --root        Print the root directory path and exit" << std::endl
    ;
}

}
}
