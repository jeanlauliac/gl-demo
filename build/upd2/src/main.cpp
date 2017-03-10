#include "cli.h"
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

namespace upd {

static const char* CACHE_FOLDER = ".upd";

template <typename ostream_t>
ostream_t& stream_join(
  ostream_t& os,
  const std::vector<std::string> elems,
  std::string sep
) {
  io::stream_string_joiner<ostream_t> joiner(os, sep);
  for (auto elem: elems) joiner.push(elem);
  return os;
}

enum class src_file_type { cpp, c };

enum class parametric_command_line_variable {
  input_files,
  output_files,
  dependency_file
};

struct parametric_command_line_part {
  parametric_command_line_part(
    std::vector<std::string> literal_args_,
    std::vector<parametric_command_line_variable> variable_args_
  ): literal_args(literal_args_), variable_args(variable_args_) {}

  std::vector<std::string> literal_args;
  std::vector<parametric_command_line_variable> variable_args;
};

struct parametric_command_line {
  std::string binary_path;
  std::vector<parametric_command_line_part> parts;
};

struct reified_command_line {
  std::string binary_path;
  std::vector<std::string> args;
};

struct command_line_parameters {
  std::string dependency_file;
  std::vector<std::string> input_files;
  std::vector<std::string> output_files;
};

void reify_command_line_arg(
  std::vector<std::string>& args,
  const parametric_command_line_variable& variable_arg,
  const command_line_parameters& parameters
) {
  switch (variable_arg) {
    case parametric_command_line_variable::input_files:
      args.insert(
        args.cend(),
        parameters.input_files.cbegin(),
        parameters.input_files.cend()
      );
      break;
    case parametric_command_line_variable::output_files:
      args.insert(
        args.cend(),
        parameters.output_files.cbegin(),
        parameters.output_files.cend()
      );
      break;
    case parametric_command_line_variable::dependency_file:
      args.push_back(parameters.dependency_file);
      break;
  }
}

reified_command_line reify_command_line(
  const parametric_command_line& base,
  const command_line_parameters& parameters
) {
  reified_command_line result;
  result.binary_path = base.binary_path;
  for (auto part: base.parts) {
    for (auto literal_arg: part.literal_args) {
      result.args.push_back(literal_arg);
    }
    for (auto variable_arg: part.variable_args) {
      reify_command_line_arg(result.args, variable_arg, parameters);
    }
  }
  return result;
}

std::string get_compile_command_line(
  const std::string& root_folder_path,
  const std::string& src_path,
  const std::string& obj_path,
  const std::string& depfile_path,
  src_file_type type
) {
  parametric_command_line base = {
    .binary_path = "clang++",
    .parts = {
      parametric_command_line_part(
        { "-c", "-o" },
        { parametric_command_line_variable::output_files }
      ),
      parametric_command_line_part(
        type == src_file_type::cpp
          ? std::vector<std::string>({ "-std=c++14" })
          : std::vector<std::string>({ "-x", "c" }),
        {}
      ),
      parametric_command_line_part(
        { "-Wall", "-fcolor-diagnostics", "-MMD", "-MF" },
        { parametric_command_line_variable::dependency_file }
      ),
      parametric_command_line_part(
        { "-I", "/usr/local/include" },
        { parametric_command_line_variable::input_files }
      )
    }
  };
  auto cli = reify_command_line(base, {
    .dependency_file = depfile_path,
    .input_files = { src_path },
    .output_files = { obj_path }
  });
  std::ostringstream oss;
  oss << cli.binary_path;
  for (auto arg: cli.args) {
    oss << " " << arg;
  }
  return oss.str();
}

bool is_file_up_to_date(
  update_log::cache& log_cache,
  file_hash_cache& hash_cache,
  const std::string& root_path,
  const std::string& local_obj_path,
  const std::string& src_path,
  const std::string& command_line
) {
  auto entry = log_cache.find(local_obj_path);
  if (entry == log_cache.end()) {
    return false;
  }
  xxhash64_stream imprint_s(0);
  imprint_s << XXH64(command_line.c_str(), command_line.size(), 0);
  imprint_s << hash_cache.hash(src_path);
  auto record = entry->second;
  for (auto local_dep_path: record.dependency_local_paths) {
    imprint_s << hash_cache.hash(root_path + '/' + local_dep_path);
  }
  return imprint_s.digest() == record.imprint;
}

void compile_src_file(
  update_log::cache& log_cache,
  file_hash_cache& hash_cache,
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
  auto read_depfile_future = std::async(std::launch::async, &depfile::read, depfile_path);
  auto ret = system(command_line.c_str());
  if (ret != 0) {
    throw std::runtime_error("compile failed");
  }
  depfile::depfile_data depfile_data = read_depfile_future.get();
  xxhash64_stream imprint_s(0);
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
  io::dir_files_reader src_files_reader_;
};

void compile_itself(const std::string& root_path) {
  std::string log_file_path = root_path + "/" + CACHE_FOLDER + "/log";
  std::string temp_log_file_path = root_path + "/" + CACHE_FOLDER + "/log_rewritten";
  update_log::cache log_cache = update_log::cache::from_log_file(log_file_path);
  file_hash_cache hash_cache;
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
  update_log::rewrite_file(log_file_path, temp_log_file_path, log_cache.records());
}

int run(int argc, char *argv[]) {
  try {
    auto cli_opts = cli::parse_options(argc, argv);
    if (cli_opts.version) {
      std::cout << "upd v0.1" << std::endl;
      return 0;
    }
    if (cli_opts.help) {
      cli::print_help();
      return 0;
    }
    auto root_path = io::find_root_path();
    if (cli_opts.root) {
      std::cout << root_path << std::endl;
      return 0;
    }
    if (cli_opts.dev) {
      // auto manifest = read_manifest<128>(root_path);
      // std::cout << inspect(manifest) << std::endl;
      return 0;
    }
    compile_itself(root_path);
    return 0;
  } catch (cli::option_parse_error error) {
    std::cerr << "upd: fatal: invalid argument: `" << error.arg << "`" << std::endl;
    return 1;
  } catch (io::cannot_find_updfile_error) {
    std::cerr << "upd: fatal: cannot find Updfile in the current directory or in any of the parent directories" << std::endl;
    return 2;
  } catch (io::ifstream_failed_error error) {
    std::cerr << "upd: fatal: failed to read file `" << error.file_path << "`" << std::endl;
    return 2;
  }
}

}

int main(int argc, char *argv[]) {
  return upd::run(argc, argv);
}
