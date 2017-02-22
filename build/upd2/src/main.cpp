#include "inspect.h"
#include "io.h"
#include "json/Lexer.h"
#include "xxhash64.h"
#include "manifest.h"
#include <array>
#include <dirent.h>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <libgen.h>
#include <map>
#include <sstream>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

static const char* CACHE_FOLDER = ".upd";

enum class src_file_type { cpp, c };

struct update_log_recorder {
  update_log_recorder(const std::string& root_path) {
    log_file_.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    log_file_.open(root_path + "/" + CACHE_FOLDER + "/log", std::ios::app);
  }

  /**
   * Record the imprint of a file that was just generated.
   */
  void record(unsigned long long imprint, const std::string& file_path) {
    log_file_ << std::setfill('0') << std::setw(2) << std::hex << imprint;
    log_file_ << " " << file_path << std::endl;
  }

private:
  std::ofstream log_file_;
};

void compile_src_file(
  update_log_recorder& log_rec,
  const std::string& src_path,
  const std::string& obj_path,
  src_file_type type
) {
  auto src_hash = upd::hash_file(0, src_path);
  std::cout << "compile... " << src_path << " (" << src_hash << ")" << std::endl;
  std::ostringstream oss;
  oss << "clang++ -c -o " << obj_path << " -Wall -fcolor-diagnostics";
  if (type == src_file_type::cpp) {
    oss << " -std=c++14";
  } else if (type == src_file_type::c) {
    oss << " -x c";
  }
  oss << " -I /usr/local/include " << src_path;
  auto str = oss.str();
  auto ret = system(str.c_str());
  if (ret != 0) {
    throw "compile failed";
  }
  auto imprint = XXH64(str.c_str(), str.size(), src_hash);
  log_rec.record(imprint, obj_path);
}

bool ends_with(const std::string& value, const std::string& ending) {
  if (ending.size() > value.size()) return false;
  return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

struct src_file {
  std::string full_path;
  std::string basename;
  src_file_type type;
};

bool get_file_type(const std::string& entname, src_file_type& type) {
  if (ends_with(entname, ".cpp")) {
    type = src_file_type::cpp;
    return true;
  }
  if (ends_with(entname, ".c")) {
    type = src_file_type::c;
    return true;
  }
  return false;
}

struct src_files_finder {
  src_files_finder(const std::string& root_path):
    src_path_(root_path + "/src"),
    src_files_reader_(src_path_) {}

  bool next(src_file& file) {
    auto ent = src_files_reader_.next();
    while (ent != nullptr) {
      std::string name(ent->d_name);
      if (get_file_type(name, file.type)) {
        file.full_path = src_path_ + "/" + name;
        file.basename = name.substr(0, name.size() - 4);
        return true;
      }
      ent = src_files_reader_.next();
    }
    return false;
  }

private:
  std::string src_path_;
  upd::io::dir_files_reader src_files_reader_;
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
  update_log_recorder log_rec(root_path);
  src_files_finder src_files(root_path);
  std::vector<std::string> obj_file_paths;
  src_file target_file;
  while (src_files.next(target_file)) {
    auto obj_path = root_path + "/dist/" + target_file.basename + ".o";
    compile_src_file(log_rec, target_file.full_path, obj_path, target_file.type);
    obj_file_paths.push_back(obj_path);
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
      // auto manifest = upd::read_manifest<128>(root_path);
      // std::cout << upd::inspect(manifest) << std::endl;
      return 0;
    }
    compile_itself(root_path);
  } catch (option_parse_error error) {
    std::cerr << "upd: fatal: invalid argument: `" << error.arg << "`" << std::endl;
    return 1;
  } catch (upd::io::cannot_find_updfile_error) {
    std::cerr << "upd: fatal: cannot find Updfile in the current directory or in any of the parent directories" << std::endl;
    return 2;
  } catch (upd::io::ifstream_failed_error error) {
    std::cerr << "upd: fatal: failed to read file `" << error.file_path << "`" << std::endl;
    return 2;
  }
}
