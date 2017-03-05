#include "depfile.h"
#include "inspect.h"
#include "io.h"
#include "json/Lexer.h"
#include "manifest.h"
#include "update_log.h"
#include "xxhash64.h"
#include <array>
#include <cstdlib>
#include <dirent.h>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <libgen.h>
#include <map>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

static const char* CACHE_FOLDER = ".upd";

template <typename ostream_t>
ostream_t& stream_join(
  ostream_t& os,
  const std::vector<std::string> elems,
  std::string sep
) {
  upd::io::stream_string_joiner<ostream_t> joiner(os, sep);
  for (auto elem: elems) joiner.push(elem);
  return os;
}

enum class src_file_type { cpp, c };

std::string get_compile_command_line(
  const std::string& root_folder_path,
  const std::string& src_path,
  const std::string& obj_path,
  const std::string& depfile_path,
  src_file_type type
) {
  std::ostringstream oss;
  oss << "clang++ -c -o " << obj_path;
  oss << " -Wall -fcolor-diagnostics";
  if (type == src_file_type::cpp) {
    oss << " -std=c++14";
  } else if (type == src_file_type::c) {
    oss << " -x c";
  }
  oss << " -MMD -MF " << depfile_path;
  oss << " -I /usr/local/include " << src_path;
  return oss.str();
}

bool is_file_up_to_date(
  upd::update_log::cache& log_cache,
  upd::file_hash_cache& hash_cache,
  const std::string& root_path,
  const std::string& local_obj_path,
  const std::string& src_path,
  const std::string& command_line
) {
  auto entry = log_cache.find(local_obj_path);
  if (entry == log_cache.end()) {
    return false;
  }
  upd::xxhash64_stream imprint_s(0);
  imprint_s << XXH64(command_line.c_str(), command_line.size(), 0);
  imprint_s << hash_cache.hash(src_path);
  auto record = entry->second;
  for (auto local_dep_path: record.dependency_local_paths) {
    imprint_s << hash_cache.hash(root_path + '/' + local_dep_path);
  }
  return imprint_s.digest() == record.imprint;
}

void compile_src_file(
  upd::update_log::cache& log_cache,
  upd::file_hash_cache& hash_cache,
  const std::string& root_path,
  const std::string& local_src_path,
  const std::string& local_obj_path,
  const std::string& depfile_path,
  src_file_type type
) {
  auto root_folder_path = root_path + '/';
  auto src_path = root_folder_path + local_src_path;
  auto obj_path = root_folder_path + local_obj_path;
  auto command_line = get_compile_command_line(root_folder_path, src_path, obj_path, depfile_path, type);
  if (is_file_up_to_date(log_cache, hash_cache, root_path, local_obj_path, src_path, command_line)) {
    return;
  }
  std::cout << "compiling: " << local_src_path << std::endl;
  //std::thread::thread read_depfile_thread(&read_depfile_thread_entry, depfile_path, std::ref(depfile_result));
  auto read_depfile_future = std::async(std::launch::async, &upd::depfile::read, depfile_path);
  auto ret = system(command_line.c_str());
  if (ret != 0) {
    throw std::runtime_error("compile failed");
  }
  upd::depfile::depfile_data depfile_data = read_depfile_future.get();
  upd::xxhash64_stream imprint_s(0);
  imprint_s << XXH64(command_line.c_str(), command_line.size(), 0);
  imprint_s << hash_cache.hash(src_path);
  std::vector<std::string> dep_local_paths;
  for (auto dep_path: depfile_data.dependency_paths) {
    if (dep_path.compare(0, root_folder_path.size(), root_folder_path) != 0) {
      throw std::runtime_error("depfile has a file out of root");
    }
    dep_local_paths.push_back(dep_path.substr(root_folder_path.size()));
    imprint_s << hash_cache.hash(dep_path);
  }
  log_cache.record(local_obj_path, {
    .imprint = imprint_s.digest(),
    .dependency_local_paths = dep_local_paths
  });
}

bool ends_with(const std::string& value, const std::string& ending) {
  if (ending.size() > value.size()) return false;
  return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

struct src_file {
  std::string local_path;
  std::string basename;
  src_file_type type;
};

bool get_file_type(const std::string& entname, src_file_type& type, std::string& basename) {
  if (ends_with(entname, ".cpp")) {
    type = src_file_type::cpp;
    basename = entname.substr(0, entname.size() - 4);
    return true;
  }
  if (ends_with(entname, ".c")) {
    type = src_file_type::c;
    basename = entname.substr(0, entname.size() - 2);
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
      if (get_file_type(name, file.type, file.basename)) {
        file.local_path = "src/" + name;
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

void compile_itself(const std::string& root_path) {
  std::string log_file_path = root_path + "/" + CACHE_FOLDER + "/log";
  std::string temp_log_file_path = root_path + "/" + CACHE_FOLDER + "/log_rewritten";
  upd::update_log::cache log_cache = upd::update_log::cache::from_log_file(log_file_path);
  upd::file_hash_cache hash_cache;
  auto depfile_path = root_path + "/.upd/depfile";
  if (mkfifo(depfile_path.c_str(), 0644) != 0 && errno != EEXIST) {
    throw std::runtime_error("cannot make depfile FIFO");
  }
  src_files_finder src_files(root_path);
  std::vector<std::string> obj_file_paths;
  src_file target_file;
  while (src_files.next(target_file)) {
    auto obj_path = "dist/" + target_file.basename + ".o";
    compile_src_file(log_cache, hash_cache, root_path, target_file.local_path, obj_path, depfile_path, target_file.type);
    obj_file_paths.push_back(root_path + '/' + obj_path);
  }
  std::cout << "linking: dist/upd" << std::endl;
  std::ostringstream oss;
  oss << "clang++ -o " << root_path << "/dist/upd "
      << "-Wall -std=c++14 -fcolor-diagnostics -L /usr/local/lib ";
  stream_join(oss, obj_file_paths, " ");
  auto ret = system(oss.str().c_str());
  if (ret != 0) {
    throw "link failed";
  }
  std::cout << "done" << std::endl;
  log_cache.close();
  upd::update_log::rewrite_file(log_file_path, temp_log_file_path, log_cache.records());
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
