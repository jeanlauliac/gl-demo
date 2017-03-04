#include "depfile.h"
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
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <vector>

static const char* CACHE_FOLDER = ".upd";

template <typename ostream_t>
struct stream_string_joiner {
  stream_string_joiner(ostream_t& os, const std::string& separator):
    os_(os), first_(true), separator_(separator) {}
  template <typename elem_t>
  stream_string_joiner<ostream_t>& push(const elem_t& elem) {
    if (!first_) os_ << separator_;
    os_ << elem;
    first_ = false;
    return *this;
  }

private:
  ostream_t& os_;
  bool first_;
  std::string separator_;
};

template <typename ostream_t>
ostream_t& stream_join(
  ostream_t& os,
  const std::vector<std::string> elems,
  std::string sep
) {
  stream_string_joiner<ostream_t> joiner(os, sep);
  for (auto elem: elems) joiner.push(elem);
  return os;
}

enum class src_file_type { cpp, c };

struct update_log_file_data {
  unsigned long long imprint;
  std::vector<std::string> dependency_local_paths;
};

enum class update_log_record_mode { append, truncate };

struct update_log_recorder {
  update_log_recorder(const std::string& log_file_path, update_log_record_mode mode) {
    log_file_.exceptions(std::ofstream::failbit | std::ofstream::badbit);
    log_file_.open(log_file_path, mode == update_log_record_mode::append ? std::ios::app : 0);
    log_file_ << std::setfill('0') << std::setw(16) << std::hex;
  }

  /**
   * Record the imprint of a file that was just generated.
   */
  void record(
    const std::string& local_file_path,
    const update_log_file_data& file_data
  ) {
    stream_string_joiner<std::ofstream> joiner(log_file_, " ");
    joiner.push(file_data.imprint).push(local_file_path);
    for (auto path: file_data.dependency_local_paths) joiner.push(path);
    log_file_ << std::endl;
  }

  void close() {
    log_file_.close();
  }

private:
  std::ofstream log_file_;
};

typedef std::unordered_map<std::string, update_log_file_data> update_log_data;

/**
 * We keep of copy of the update log in memory. New additions are written right
 * away to the log so that even if the process crashes we keep track of
 * what files are already updated.
 */
struct update_log_cache {
  update_log_cache(const std::string& log_file_path, const update_log_data& data):
    recorder_(log_file_path, update_log_record_mode::append), data_(data) {}

  update_log_data::iterator find(const std::string& local_file_path) {
    return data_.find(local_file_path);
  }

  update_log_data::iterator end() {
    return data_.end();
  }

  void record(
    const std::string& local_file_path,
    const update_log_file_data& file_data
  ) {
    recorder_.record(local_file_path, file_data);
    data_[local_file_path] = file_data;
  }

  void close() {
    recorder_.close();
  }

  const update_log_data& data() const {
    return data_;
  }

  static update_log_data data_from_log_file(const std::string& log_file_path) {
    update_log_data data;
    std::ifstream log_file;
    log_file.exceptions(std::ifstream::badbit);
    log_file.open(log_file_path);
    std::string line, local_file_path, dep_file_path;
    std::istringstream iss;
    update_log_file_data file_data;
    while (std::getline(log_file, line)) {
      file_data.dependency_local_paths.clear();
      iss.str(line);
      iss.clear();
      iss >> std::hex;
      iss >> file_data.imprint >> local_file_path;
      while (!iss.eof()) {
        iss >> dep_file_path;
        file_data.dependency_local_paths.push_back(dep_file_path);
      }
      data[local_file_path] = file_data;
    }
    return data;
  }

  static update_log_cache from_log_file(const std::string& log_file_path) {
    auto data = data_from_log_file(log_file_path);
    return update_log_cache(log_file_path, data);
  }

private:
  update_log_recorder recorder_;
  update_log_data data_;
};

/**
 * Once all the updates has happened, we have appended the new entries to the
 * update log, but there may be duplicates. To finalize the log, we rewrite
 * it from scratch, and we replace the existing log. `rename` is normally an
 * atomic operation, so we ensure no data is lost.
 */
void rewrite_log_file(
  const std::string& log_file_path,
  const std::string& temporary_log_file_path,
  const update_log_data& data
) {
  update_log_recorder recorder(temporary_log_file_path, update_log_record_mode::truncate);
  for (auto file_data: data) {
    recorder.record(file_data.first, file_data.second);
  }
  recorder.close();
  if (rename(temporary_log_file_path.c_str(), log_file_path.c_str()) != 0) {
    throw std::runtime_error("failed to overwrite log file");
  }
}

struct depfile_thread_result {
  upd::depfile::depfile_data data;
  bool has_error;
};

void read_depfile_thread_entry(
  const std::string& depfile_path,
  depfile_thread_result& result
) {
  try {
    upd::depfile::read(depfile_path, result.data);
    result.has_error = false;
  } catch (upd::depfile::parse_error error) {
    std::cerr << "upd: fatal: while reading depfile: " << error.message() << std::endl;
    result.has_error = true;
  }
}

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
  update_log_cache& log_cache,
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
  update_log_cache& log_cache,
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
  depfile_thread_result depfile_result;
  std::thread::thread read_depfile_thread(&read_depfile_thread_entry, depfile_path, std::ref(depfile_result));
  auto ret = system(command_line.c_str());
  if (ret != 0) {
    throw std::runtime_error("compile failed");
  }
  read_depfile_thread.join();
  if (depfile_result.has_error) {
    throw std::runtime_error("depfile reading thread had errors");
  }
  upd::xxhash64_stream imprint_s(0);
  imprint_s << XXH64(command_line.c_str(), command_line.size(), 0);
  imprint_s << hash_cache.hash(src_path);
  std::vector<std::string> dep_local_paths;
  for (auto dep_path: depfile_result.data.dependency_paths) {
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
  update_log_cache log_cache = update_log_cache::from_log_file(log_file_path);
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
  rewrite_log_file(log_file_path, temp_log_file_path, log_cache.data());
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
