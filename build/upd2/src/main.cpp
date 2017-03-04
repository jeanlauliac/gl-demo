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

/**
 * We want to read the stream on a character basis, but do some caching to avoid
 * too many I/O calls. We use templating to avoid using virtual functions as
 * much as possible.
 */
template <typename istream_t>
struct istream_char_reader {
  istream_char_reader(istream_t& stream):
    stream_(stream), next_(buffer_), end_(buffer_) {}

  bool next(char& c) {
    if (next_ >= end_) {
      stream_.read(buffer_, sizeof(buffer_));
      end_ = buffer_ + stream_.gcount();
      next_ = buffer_;
    }
    if (next_ >= end_) {
      return false;
    }
    c = *next_;
    ++next_;
    return true;
  }

private:
  istream_t& stream_;
  char* next_;
  char* end_;
  char buffer_[1 << 12];
};

/**
 * Transform a stream of characters into tokens. We have to look one character
 * ahead to know when a token ends. For example a string is ended when we know
 * the next character is space.
 */
template <typename istream_t>
struct depfile_tokenizer {
  depfile_tokenizer(istream_t& stream): char_reader_(stream) { read_(); }

  template <typename handler_t, typename retval_t>
  retval_t next(handler_t& handler) {
    while (good_ && (c_ == ' ')) read_();
    if (!good_) { return handler.end(); };
    if (c_ == ':') { read_(); return handler.colon(); }
    if (c_ == '\n') { read_(); return handler.new_line(); }
    std::ostringstream oss;
    while (good_ && !(c_ == ' ' || c_ == ':' || c_ == '\n')) {
      if (c_ == '\\') { read_(); }
      oss.put(c_);
      read_();
    }
    auto str = oss.str();
    if (str.size() == 0) {
      throw std::runtime_error("string token of size zero, parser is broken");
    }
    return handler.string(str);
  }

private:
  void read_() {
    read_unescaped_();
    if (!good_ || c_ != '\\') return;
    read_unescaped_();
    if (!good_) {
      throw std::runtime_error("expected character after escape sequence `\\`");
    }
    if (c_ == '\n') {
      c_ = ' ';
    }
  }
  void read_unescaped_() { good_ = char_reader_.next(c_); }
  char c_;
  bool good_;
  istream_char_reader<istream_t> char_reader_;
};

struct depfile_data {
  std::string target_path;
  std::vector<std::string> dependency_paths;
};

struct depfile_parse_error {
  depfile_parse_error(const std::string& message): message_(message) {}
  std::string message() const { return message_; };
private:
  std::string message_;
};

/**
 * State machine that updates the depfile data for each type of token.
 */
struct parse_depfile_token_handler {
  parse_depfile_token_handler(depfile_data& data):
    data_(data), state_(state_t::read_target) {}
  bool end() {
    if (!(state_ == state_t::read_dep || state_ == state_t::done)) {
      throw depfile_parse_error("unexpected end");
    }
    state_ = state_t::done;
    return false;
  }
  bool colon() {
    if (state_ != state_t::read_colon) {
      throw depfile_parse_error("unexpected colon operator");
    }
    state_ = state_t::read_dep;
    return true;
  }
  bool string(const std::string& file_path) {
    if (state_ == state_t::read_target) {
      data_.target_path = file_path;
      state_ = state_t::read_colon;
      return true;
    }
    if (state_ == state_t::read_dep) {
      data_.dependency_paths.push_back(file_path);
      return true;
    }
    throw depfile_parse_error("unexpected string `" + file_path + "`");
  }
  bool new_line() {
    if (state_ == state_t::read_target) {
      return true;
    }
    if (state_ != state_t::read_dep) {
      throw depfile_parse_error("unexpected newline");
    }
    state_ = state_t::done;
    return true;
  }

private:
  enum class state_t { read_target, read_colon, read_dep, done };
  depfile_data& data_;
  state_t state_;
};

/**
 * Parse a depfile stream. A 'depfile' is a Makefile-like representation of file
 * dependencies. For example it is generated by the -MF option of gcc or clang,
 * in which case it describes what source and headers a compiled object file
 * depends on. Ex:
 *
 *     foo.o: foo.cpp \
 *       some_header.h \
 *       another_header.h
 *
 */
template <typename istream_t>
depfile_data parse_depfile(istream_t& stream, depfile_data& data) {
  depfile_tokenizer<istream_t> tokens(stream);
  parse_depfile_token_handler handler(data);
  while (tokens.template next<parse_depfile_token_handler, bool>(handler));
  return data;
}

void read_depfile(const std::string& depfile_path, depfile_data& data) {
  std::ifstream depfile;
  depfile.exceptions(std::ifstream::badbit);
  depfile.open(depfile_path);
  auto deps = parse_depfile(depfile, data);
}

struct depfile_thread_result {
  depfile_data data;
  bool has_error;
};

void read_depfile_thread_entry(
  const std::string& depfile_path,
  depfile_thread_result& result
) {
  try {
    read_depfile(depfile_path, result.data);
    result.has_error = false;
  } catch (depfile_parse_error error) {
    std::cerr << "upd: fatal: while reading depfile: " << error.message() << std::endl;
    result.has_error = true;
  }
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
  std::cout << "compiling: " << local_src_path << std::endl;
  std::ostringstream oss;
  oss << "clang++ -c -o " << root_folder_path << local_obj_path;
  oss << " -Wall -fcolor-diagnostics";
  if (type == src_file_type::cpp) {
    oss << " -std=c++14";
  } else if (type == src_file_type::c) {
    oss << " -x c";
  }
  depfile_thread_result depfile_result;
  std::thread::thread read_depfile_thread(&read_depfile_thread_entry, depfile_path, std::ref(depfile_result));
  oss << " -MMD -MF " << depfile_path;
  oss << " -I /usr/local/include " << src_path;
  auto str = oss.str();
  auto ret = system(str.c_str());
  if (ret != 0) {
    throw std::runtime_error("compile failed");
  }
  read_depfile_thread.join();
  if (depfile_result.has_error) {
    throw std::runtime_error("depfile reading thread had errors");
  }
  auto imprint = XXH64(str.c_str(), str.size(), 0);
  imprint ^= hash_cache.hash(src_path);
  std::vector<std::string> dep_local_paths;
  for (auto dep_path: depfile_result.data.dependency_paths) {
    if (dep_path.compare(0, root_folder_path.size(), root_folder_path) != 0) {
      throw std::runtime_error("depfile has a file out of root");
    }
    dep_local_paths.push_back(dep_path.substr(root_folder_path.size()));
    imprint ^= hash_cache.hash(dep_path);
  }
  log_cache.record(local_obj_path, {
    .imprint = imprint,
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
