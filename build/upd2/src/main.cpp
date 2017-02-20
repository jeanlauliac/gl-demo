#include "inspect.h"
#include "io.h"
#include "json/Lexer.h"
#include "manifest.h"
#include <dirent.h>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <libgen.h>
#include <map>
#include <sstream>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

void compile_cpp_file(const std::string& cpp_path, const std::string& obj_path) {
  std::cout << "compile... " << cpp_path << std::endl;
  auto ret = system((
    std::string("clang++ -c -o ") + obj_path +
    " -Wall -std=c++14 -fcolor-diagnostics " +
    "-I /usr/local/include " + cpp_path
  ).c_str());
  if (ret != 0) {
    throw "compile failed";
  }
}

bool ends_with(const std::string& value, const std::string& ending) {
  if (ending.size() > value.size()) return false;
  return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

std::vector<std::string> find_all_cpp_files(const std::string& root_path) {
  std::vector<std::string> result;
  std::string src_path = root_path + "/src";
  upd::io::dir_files src_files(src_path);
  auto ent = src_files.next();
  while (ent != nullptr) {
    std::string name(ent->d_name);
    if (ends_with(name, ".cpp")) {
      result.push_back(src_path + "/" + name);
    }
    ent = src_files.next();
  }
  return result;
}

void compile_itself(const std::string& root_path) {
  find_all_cpp_files(root_path);
  return;
  auto main_obj_path = root_path + "/dist/main.o";
  auto io_obj_path = root_path + "/dist/io.o";
  auto mn_obj_path = root_path + "/dist/manifest.o";
  auto in_obj_path = root_path + "/dist/inspect.o";
  compile_cpp_file(root_path + "/src/main.cpp", main_obj_path);
  compile_cpp_file(root_path + "/src/io.cpp", io_obj_path);
  compile_cpp_file(root_path + "/src/manifest.cpp", mn_obj_path);
  compile_cpp_file(root_path + "/src/inspect.cpp", in_obj_path);
  std::cout << "link..." << std::endl;
  auto ret = system((
    std::string("clang++ -o ") + root_path + "/dist/upd " +
    "-Wall -std=c++14 -fcolor-diagnostics -L /usr/local/lib " +
    main_obj_path + " " + io_obj_path + " " + mn_obj_path + " " + in_obj_path
  ).c_str());
  if (ret != 0) {
    throw "link failed";
  }
  std::cout << "done" << std::endl;
}

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

options parse_options(int argc, char *argv[]) {
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
    << "usage: upd [targets] [options]" << std::endl
    << "options:" << std::endl
    << "  --version     print semantic version and exit" << std::endl
    << "  --help        print usage help and exit" << std::endl
    << "  --root        print the root directory path and exit" << std::endl
    ;
}

int main(int argc, char *argv[]) {
  try {
    auto cli_opts = parse_options(argc, argv);
    if (cli_opts.version) {
      std::cout << "upd v0.1" << std::endl;
      return 0;
    }
    if (cli_opts.help) {
      print_help();
      return 0;
    }
    auto root_path = upd::io::find_root_path();
    if (cli_opts.root) {
      std::cout << root_path << std::endl;
      return 0;
    }
    if (cli_opts.dev) {
      auto manifest = upd::read_manifest<128>(root_path);
      std::cout << upd::inspect(manifest) << std::endl;
      return 0;
    }
    compile_itself(root_path);
  } catch (option_parse_error error) {
    std::cerr << "upd: fatal: invalid argument: `" << error.arg << "`" << std::endl;
    return 1;
  } catch (upd::io::cannot_find_updfile_error) {
    std::cerr << "upd: fatal: cannot find Updfile in the current directory or in any of the parent directories" << std::endl;
    return 2;
  }
}
