#include "lib/cli.h"
#include "lib/depfile.h"
#include "lib/inspect.h"
#include "lib/io.h"
#include "lib/json/Lexer.h"
#include "lib/manifest.h"
#include "lib/update_log.h"
#include "lib/xxhash64.h"
#include <array>
#include <cstdlib>
#include <dirent.h>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <libgen.h>
#include <map>
#include <queue>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace upd {

static const std::string CACHE_FOLDER = ".upd";

template <typename ostream_t>
ostream_t& stream_join(
  ostream_t& os,
  const std::vector<std::string> elems,
  std::string sep
) {
  io::stream_string_joiner<ostream_t> joiner(os, sep);
  for (auto const& elem: elems) joiner.push(elem);
  return os;
}

enum class src_file_type { cpp, c, cpp_test };

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
  for (auto const& part: base.parts) {
    for (auto const& literal_arg: part.literal_args) {
      result.args.push_back(literal_arg);
    }
    for (auto const& variable_arg: part.variable_args) {
      reify_command_line_arg(result.args, variable_arg, parameters);
    }
  }
  return result;
}

parametric_command_line get_compile_command_line(src_file_type type) {
  return {
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
}

XXH64_hash_t hash_command_line(const reified_command_line& command_line) {
  xxhash64_stream cli_hash(0);
  cli_hash << upd::hash(command_line.binary_path);
  cli_hash << upd::hash(command_line.args);
  return cli_hash.digest();
}

XXH64_hash_t hash_files(
  file_hash_cache& hash_cache,
  const std::string& root_path,
  const std::vector<std::string>& local_paths
) {
  xxhash64_stream imprint_s(0);
  for (auto const& local_path: local_paths) {
    imprint_s << hash_cache.hash(root_path + '/' + local_path);
  }
  return imprint_s.digest();
}

XXH64_hash_t get_target_imprint(
  file_hash_cache& hash_cache,
  const std::string& root_path,
  const std::vector<std::string>& local_src_paths,
  std::vector<std::string> dependency_local_paths,
  const reified_command_line& command_line
) {
  xxhash64_stream imprint_s(0);
  imprint_s << hash_command_line(command_line);
  imprint_s << hash_files(hash_cache, root_path, local_src_paths);
  imprint_s << hash_files(hash_cache, root_path, dependency_local_paths);
  return imprint_s.digest();
}

bool is_file_up_to_date(
  update_log::cache& log_cache,
  file_hash_cache& hash_cache,
  const std::string& root_path,
  const std::string& local_target_path,
  const std::vector<std::string>& local_src_paths,
  const reified_command_line& command_line
) {
  auto entry = log_cache.find(local_target_path);
  if (entry == log_cache.end()) {
    return false;
  }
  auto const& record = entry->second;
  auto new_imprint = get_target_imprint(
    hash_cache,
    root_path,
    local_src_paths,
    record.dependency_local_paths,
    command_line
  );
  if (new_imprint != record.imprint) {
    return false;
  }
  auto new_hash = hash_cache.hash(root_path + "/" + local_target_path);
  return new_hash == record.hash;
}

void run_command_line(const std::string& root_path, reified_command_line target) {
  pid_t child_pid = fork();
  if (child_pid == 0) {
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>(target.binary_path.c_str()));
    for (auto const& arg: target.args) {
      argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);
    if (chdir(root_path.c_str()) != 0) {
      std::cerr << "upd: chdir() failed" << std::endl;
      _exit(1);
    }
    execvp(target.binary_path.c_str(), argv.data());
    std::cerr << "upd: execvp() failed" << std::endl;
    _exit(1);
  }
  if (child_pid < 0) {
    throw std::runtime_error("command line failed");
  }
  int status;
  if (waitpid(child_pid, &status, 0) != child_pid) {
    throw std::runtime_error("waitpid failed");
  }
  if (!WIFEXITED(status)) {
    throw std::runtime_error("process did not terminate normally");
  }
  if (WEXITSTATUS(status) != 0) {
    throw std::runtime_error("process terminated with errors");
  }
}

void update_file(
  update_log::cache& log_cache,
  file_hash_cache& hash_cache,
  const std::string& root_path,
  const parametric_command_line& param_cli,
  const std::vector<std::string>& local_src_paths,
  const std::string& local_target_path,
  const std::string& local_depfile_path
) {
  auto root_folder_path = root_path + '/';
  auto command_line = reify_command_line(param_cli, {
    .dependency_file = local_depfile_path,
    .input_files = local_src_paths,
    .output_files = { local_target_path }
  });
  if (is_file_up_to_date(log_cache, hash_cache, root_path, local_target_path, local_src_paths, command_line)) {
    return;
  }
  std::cout << "updating: " << local_target_path << std::endl;
  auto depfile_path = root_path + '/' + local_depfile_path;
  auto read_depfile_future = std::async(std::launch::async, &depfile::read, depfile_path);
  std::ofstream depfile_writer(depfile_path);
  run_command_line(root_path, command_line);
  hash_cache.invalidate(root_path + '/' + local_target_path);
  depfile_writer.close();
  std::unique_ptr<depfile::depfile_data> depfile_data = read_depfile_future.get();
  std::vector<std::string> dep_local_paths;
  if (depfile_data) {
    for (auto dep_path: depfile_data->dependency_paths) {
      if (dep_path.at(0) == '/') {
        if (dep_path.compare(0, root_folder_path.size(), root_folder_path) != 0) {
          throw std::runtime_error("depfile has a file out of root");
        }
        dep_path = dep_path.substr(root_folder_path.size());
      }
      dep_local_paths.push_back(dep_path);
    }
  }
  auto new_imprint = get_target_imprint(
    hash_cache,
    root_path,
    local_src_paths,
    dep_local_paths,
    command_line
  );
  auto new_hash = hash_cache.hash(local_target_path);
  log_cache.record(local_target_path, {
    .dependency_local_paths = dep_local_paths,
    .hash = new_hash,
    .imprint = new_imprint
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
  if (ends_with(entname, ".cppt")) {
    type = src_file_type::cpp_test;
    basename = entname.substr(0, entname.size() - 5);
    return true;
  }
  return false;
}

struct src_files_finder {
  src_files_finder(const std::string& root_path):
    root_path_(root_path),
    src_path_suffix_("lib/"),
    src_files_reader_(root_path_ + "/src/lib") {}

  bool next(src_file& file) {
    while (true) {
      auto ent = src_files_reader_.next();
      while (ent != nullptr) {
        std::string name(ent->d_name);
        if (ent->d_type == DT_DIR) {
          if (!name.empty() && name[0] != '.') {
            pending_src_path_folders_.push(src_path_suffix_ + name);
          }
        } else {
          auto src_local_path = src_path_suffix_ + name;
          file.local_path = "src/" + src_local_path;
          if (get_file_type(src_local_path, file.type, file.basename)) {
            return true;
          }
        }
        ent = src_files_reader_.next();
      }
      if (pending_src_path_folders_.empty()) {
        return false;
      }
      src_path_suffix_ = pending_src_path_folders_.front() + '/';
      pending_src_path_folders_.pop();
      src_files_reader_.open(root_path_ + "/src/" + src_path_suffix_);
    }
  }

private:
  const std::string root_path_;
  std::string src_path_suffix_;
  io::dir_files_reader src_files_reader_;
  std::queue<std::string> pending_src_path_folders_;
};

parametric_command_line get_cppt_command_line(const std::string& root_path) {
  return {
    .binary_path = root_path + "/tools/compile_test.js",
    .parts = {
      parametric_command_line_part({}, {
        parametric_command_line_variable::input_files,
        parametric_command_line_variable::output_files,
        parametric_command_line_variable::dependency_file
      })
    }
  };
}

parametric_command_line get_link_command_line() {
  return {
    .binary_path = "clang++",
    .parts = {
      parametric_command_line_part(
        { "-o" },
        { parametric_command_line_variable::output_files }
      ),
      parametric_command_line_part(
        { "-Wall", "-fcolor-diagnostics", "-std=c++14", "-L", "/usr/local/lib" },
        { parametric_command_line_variable::input_files }
      )
    }
  };
}

void compile_itself(const std::string& root_path) {
  std::string log_file_path = root_path + "/" + CACHE_FOLDER + "/log";
  std::string temp_log_file_path = root_path + "/" + CACHE_FOLDER + "/log_rewritten";
  update_log::cache log_cache = update_log::cache::from_log_file(log_file_path);
  file_hash_cache hash_cache;
  auto local_depfile_path = CACHE_FOLDER + "/depfile";
  auto depfile_path = root_path + '/' + local_depfile_path;
  if (mkfifo(depfile_path.c_str(), 0644) != 0 && errno != EEXIST) {
    throw std::runtime_error("cannot make depfile FIFO");
  }
  src_files_finder src_files(root_path);
  std::vector<std::string> local_obj_file_paths;
  std::vector<std::string> local_test_cpp_file_basenames;
  src_file target_file;
  while (src_files.next(target_file)) {
    if (target_file.type == src_file_type::cpp_test) {
      auto local_cpp_path = "dist/" + target_file.basename + ".cpp";
      auto pcli = get_cppt_command_line(root_path);
      update_file(log_cache, hash_cache, root_path, pcli, { target_file.local_path }, local_cpp_path, local_depfile_path);
      local_test_cpp_file_basenames.push_back(target_file.basename);
    } else {
      auto local_obj_path = "dist/" + target_file.basename + ".o";
      auto pcli = get_compile_command_line(target_file.type);
      update_file(log_cache, hash_cache, root_path, pcli, { target_file.local_path }, local_obj_path, local_depfile_path);
      local_obj_file_paths.push_back(local_obj_path);
    }
  }

  auto cpp_pcli = get_compile_command_line(src_file_type::cpp);
  update_file(log_cache, hash_cache, root_path, cpp_pcli, { "src/main.cpp" }, "dist/main.o", local_depfile_path);
  local_obj_file_paths.push_back("dist/main.o");


  for (auto const& basename: local_test_cpp_file_basenames) {
    auto local_path = "dist/" + basename + ".cpp";;
    auto local_obj_path = "dist/" + basename + ".o";
    auto pcli = get_compile_command_line(src_file_type::cpp);
    update_file(log_cache, hash_cache, root_path, pcli, { local_path }, local_obj_path, local_depfile_path);
    auto local_bin_path = "dist/" + basename;
    pcli = get_link_command_line();
    update_file(log_cache, hash_cache, root_path, pcli, { local_obj_path }, local_bin_path, local_depfile_path);
  }

  auto pcli = get_link_command_line();
  update_file(log_cache, hash_cache, root_path, pcli, local_obj_file_paths, "dist/upd", local_depfile_path);

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
  } catch (update_log::corruption_error) {
    std::cerr << "upd: fatal: update log is corrupted; try deleting the `.upd/log` file" << std::endl;
    return 2;
  }
}

}

int main(int argc, char *argv[]) {
  return upd::run(argc, argv);
}
