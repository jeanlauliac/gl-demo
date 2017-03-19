#pragma once

#include <iostream>

namespace upd {
namespace cli {

/**
 * The basic action that the program should do.
 */
enum class action {
  dot_graph,
  help,
  root,
  update,
  version,
};

struct options {
  options(): color_diagnostics(false), action(action::update) {};
  bool color_diagnostics;
  action action;
};

struct incompatible_options_error {
  incompatible_options_error(const std::string& first_option, const std::string& last_option):
    first_option(first_option), last_option(last_option) {}
  std::string first_option;
  std::string last_option;
};

struct unexpected_argument_error {
  unexpected_argument_error(const std::string& arg): arg(arg) {}
  std::string arg;
};

options parse_options(int argc, const char* const argv[]);
void print_help();

template <typename OStream>
OStream& ansi_sgr(OStream& os, int sgr_code, bool use_color) {
  return os << "\033[" << sgr_code << "m";
}

template <typename OStream>
OStream& fatal_error(OStream& os, bool use_color) {
  os << "upd: ";
  ansi_sgr(os, 31, use_color) << "fatal:";
  return ansi_sgr(os, 0, use_color) << ' ';
}

}
}
