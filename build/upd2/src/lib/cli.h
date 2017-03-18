#pragma once

#include <iostream>

namespace upd {
namespace cli {

struct options {
  options(): color(false), dev(false), help(false),
    root(false), version(false) {};
  bool color;
  bool dev;
  bool help;
  bool root;
  bool version;
};

struct option_parse_error {
  option_parse_error(const std::string& arg): arg(arg) {}
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
