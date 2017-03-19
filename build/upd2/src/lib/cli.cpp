#include "cli.h"

namespace upd {
namespace cli {

void setup_action(
  options& result,
  std::string& current_action_arg,
  const std::string& arg,
  const action new_action
) {
  if (!current_action_arg.empty()) {
    throw incompatible_options_error(current_action_arg, arg);
  }
  current_action_arg = arg;
  result.action = new_action;
}

options parse_options(int argc, const char* const argv[]) {
  options result;
  std::string action_arg;
  for (++argv, --argc; argc > 0; ++argv, --argc) {
    const auto arg = std::string(*argv);
    if (arg == "--root") {
      setup_action(result, action_arg, arg, action::root);
    } else if (arg == "--version") {
      setup_action(result, action_arg, arg, action::version);
    } else if (arg == "--help") {
      setup_action(result, action_arg, arg, action::help);
    } else if (arg == "--dot-graph") {
      setup_action(result, action_arg, arg, action::dot_graph);
    } else if (arg == "--color-diagnostics") {
      result.color_diagnostics = true;
    } else {
      throw unexpected_argument_error(arg);
    }
  }
  return result;
}

void print_help() {
  std::cout << R"HELP(usage: upd [options] [targets]

Operations
  --help                  Output usage help
  --root                  Output the root directory path
  --version               Output the semantic version numbers
  --dot-graph             Output a DOT-formatted graph of the output files

General options
  --color-diagnostics     Use ANSI color escape codes to stderr
)HELP";
}

}
}
