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

struct target_file {
  std::string full_path;
  std::string basename;
};

struct cpp_files_finder {
  cpp_files_finder(const std::string& root_path):
    src_path_(root_path + "/src"),
    src_files_reader_(src_path_) {}

  target_file* next() {
    auto ent = src_files_reader_.next();
    while (ent != nullptr) {
      std::string name(ent->d_name);
      if (ends_with(name, ".cpp")) {
        current_file_.full_path = src_path_ + "/" + name;
        current_file_.basename = name.substr(0, name.size() - 4);
        break;
      }
      ent = src_files_reader_.next();
    }
    if (ent == nullptr) {
      return nullptr;
    }
    return &current_file_;
  }

private:
  std::string src_path_;
  upd::io::dir_files_reader src_files_reader_;
  target_file current_file_;
};

std::ostream& stream_join(
  std::ostream& os,
  const std::vector<std::string> elems,
  std::string sep
) {
  bool first = true;
  for (auto elem : elems) {
    if (!first) os << " ";
    os << elem;
    first = false;
  }
  return os;
}

void compile_itself(const std::string& root_path) {
  cpp_files_finder cpp_files(root_path);
  std::vector<std::string> obj_file_paths;
  auto cpp_file = cpp_files.next();
  while (cpp_file != nullptr) {
    auto obj_path = root_path + "/dist/" + cpp_file->basename + ".o";
    compile_cpp_file(cpp_file->full_path, obj_path);
    obj_file_paths.push_back(obj_path);
    cpp_file = cpp_files.next();
  }
  std::cout << "link..." << std::endl;
  std::ostringstream oss;
  oss << "clang++ -o " << root_path << "/dist/upd "
      << "-Wall -std=c++14 -fcolor-diagnostics -L /usr/local/lib ";
  stream_join(oss, obj_file_paths, " ");
  auto ret = system(oss.str().c_str());
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
