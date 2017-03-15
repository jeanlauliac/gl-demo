#pragma once

#include <iostream>

namespace upd {
namespace cli {

struct options {
  options(): dev(false), help(false), root(false), version(false) {};
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

}
}
