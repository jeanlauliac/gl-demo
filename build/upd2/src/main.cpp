#include "lib/cli.h"
#include "lib/command_line_template.h"
#include "lib/depfile.h"
#include "lib/inspect.h"
#include "lib/io.h"
#include "lib/json/Lexer.h"
#include "lib/manifest.h"
#include "lib/path.h"
#include "lib/update.h"
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

enum class src_file_type { cpp, c, cpp_test };

command_line_template get_compile_command_line(src_file_type type) {
  return {
    .binary_path = "clang++",
    .parts = {
      command_line_template_part(
        { "-c", "-o" },
        { command_line_template_variable::output_files }
      ),
      command_line_template_part(
        type == src_file_type::cpp
          ? std::vector<std::string>({ "-std=c++14" })
          : std::vector<std::string>({ "-x", "c" }),
        {}
      ),
      command_line_template_part(
        { "-Wall", "-fcolor-diagnostics", "-MMD", "-MF" },
        { command_line_template_variable::dependency_file }
      ),
      command_line_template_part(
        { "-I", "/usr/local/include" },
        { command_line_template_variable::input_files }
      )
    }
  };
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
          file.local_path = "src/" + src_path_suffix_ + name;
          if (get_file_type(file.local_path, file.type, file.basename)) {
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

command_line_template get_cppt_command_line(const std::string& root_path) {
  return {
    .binary_path = "tools/compile_test.js",
    .parts = {
      command_line_template_part({}, {
        command_line_template_variable::input_files,
        command_line_template_variable::output_files,
        command_line_template_variable::dependency_file
      })
    }
  };
}

command_line_template get_index_tests_command_line() {
  return {
    .binary_path = "tools/index_tests.js",
    .parts = {
      command_line_template_part({}, {
        command_line_template_variable::output_files,
        command_line_template_variable::dependency_file,
        command_line_template_variable::input_files
      })
    }
  };
}

command_line_template get_link_command_line() {
  return {
    .binary_path = "clang++",
    .parts = {
      command_line_template_part(
        { "-o" },
        { command_line_template_variable::output_files }
      ),
      command_line_template_part(
        { "-Wall", "-fcolor-diagnostics", "-std=c++14", "-L", "/usr/local/lib" },
        { command_line_template_variable::input_files }
      )
    }
  };
}

struct output_file {
  command_line_template command_line;
  std::vector<std::string> local_input_file_paths;
};

struct unknown_target_error {
  unknown_target_error(const std::string& relative_path):
    relative_path(relative_path) {}
  std::string relative_path;
};

void update_file_recursively(
  update_log::cache& log_cache,
  file_hash_cache& hash_cache,
  const std::string& root_path,
  const std::unordered_map<std::string, output_file>& output_files_by_path,
  const std::pair<std::string, output_file>& target_descriptor,
  const std::string& local_depfile_path
) {
  for (auto const& local_input_path: target_descriptor.second.local_input_file_paths) {
    auto search = output_files_by_path.find(local_input_path);
    if (search == output_files_by_path.end()) {
      continue;
    }
    update_file_recursively(
      log_cache,
      hash_cache,
      root_path,
      output_files_by_path,
      *search,
      local_depfile_path
    );
  }
  update_file(
    log_cache,
    hash_cache,
    root_path,
    target_descriptor.second.command_line,
    target_descriptor.second.local_input_file_paths,
    target_descriptor.first,
    local_depfile_path
  );
}

template <typename OStream>
void output_dot_graph(
  OStream& os,
  const std::unordered_map<std::string, output_file>& output_files_by_path
) {
  os << "# generated with `upd --dot-graph`" << std::endl
    << "digraph upd {" << std::endl
    << "  rankdir=\"LR\";" << std::endl;
  for (auto const& entry: output_files_by_path) {
    //os << entry.first << ": (" << entry.second.command_line.binary_path << ") ";
    for (auto const& input_path: entry.second.local_input_file_paths) {
      os << "  \"" << input_path << "\" -> \""
        << entry.first << "\" [label=\""
        << entry.second.command_line.binary_path << "\"];" << std::endl;
    }
  }
  os << "}" << std::endl;
}

void compile_itself(
  const std::string& root_path,
  const std::string& working_path,
  bool print_graph,
  const std::vector<std::string>& relative_target_paths
) {
  std::string log_file_path = root_path + "/" + CACHE_FOLDER + "/log";
  std::string temp_log_file_path = root_path + "/" + CACHE_FOLDER + "/log_rewritten";
  update_log::cache log_cache = update_log::cache::from_log_file(log_file_path);
  file_hash_cache hash_cache;
  auto local_depfile_path = CACHE_FOLDER + "/depfile";
  auto depfile_path = root_path + '/' + local_depfile_path;
  if (mkfifo(depfile_path.c_str(), 0644) != 0 && errno != EEXIST) {
    throw std::runtime_error("cannot make depfile FIFO");
  }
  std::unordered_map<std::string, output_file> output_files_by_path;
  src_files_finder src_files(root_path);
  std::vector<std::string> local_obj_file_paths;
  std::vector<std::string> local_test_cpp_file_basenames;
  std::vector<std::string> local_test_cppt_file_paths;
  src_file target_file;
  auto cppt_cli = get_cppt_command_line(root_path);
  while (src_files.next(target_file)) {
    if (target_file.type == src_file_type::cpp_test) {
      auto local_cpp_path = "dist/" + target_file.basename + ".cpp";
      output_files_by_path[local_cpp_path] = {
        .command_line = cppt_cli,
        .local_input_file_paths = { target_file.local_path }
      };
      local_test_cpp_file_basenames.push_back(target_file.basename);
      local_test_cppt_file_paths.push_back(target_file.local_path);
    } else {
      auto local_obj_path = "dist/" + target_file.basename + ".o";
      auto pcli = get_compile_command_line(target_file.type);
      output_files_by_path[local_obj_path] = {
        .command_line = pcli,
        .local_input_file_paths = { target_file.local_path }
      };
      local_obj_file_paths.push_back(local_obj_path);
    }
  }

  auto pcli = get_index_tests_command_line();
  output_files_by_path["dist/tests.cpp"] = {
    .command_line = pcli,
    .local_input_file_paths = { local_test_cppt_file_paths }
  };
  local_test_cpp_file_basenames.push_back("tests");

  auto cpp_pcli = get_compile_command_line(src_file_type::cpp);
  auto local_upd_object_file_paths = local_obj_file_paths;
  output_files_by_path["dist/src/main.o"] = {
    .command_line = cpp_pcli,
    .local_input_file_paths = { "src/main.cpp" }
  };
  local_upd_object_file_paths.push_back("dist/src/main.o");

  auto local_test_object_file_paths = local_obj_file_paths;
  output_files_by_path["dist/tools/lib/testing.o"] = {
    .command_line = cpp_pcli,
    .local_input_file_paths = { "tools/lib/testing.cpp" }
  };
  local_test_object_file_paths.push_back("dist/tools/lib/testing.o");

  for (auto const& basename: local_test_cpp_file_basenames) {
    auto local_path = "dist/" + basename + ".cpp";
    auto local_obj_path = "dist/" + basename + ".o";
    auto pcli = get_compile_command_line(src_file_type::cpp);
    output_files_by_path[local_obj_path] = {
      .command_line = cpp_pcli,
      .local_input_file_paths = { local_path }
    };
    local_test_object_file_paths.push_back(local_obj_path);
  }

  pcli = get_link_command_line();
  output_files_by_path["dist/upd"] = {
    .command_line = pcli,
    .local_input_file_paths = { local_upd_object_file_paths }
  };
  output_files_by_path["dist/tests"] = {
    .command_line = pcli,
    .local_input_file_paths = { local_test_object_file_paths }
  };

  if (print_graph) {
    output_dot_graph(std::cout, output_files_by_path);
    return;
  }

  std::vector<std::string> local_target_paths;
  for (auto const& relative_path: relative_target_paths) {
    auto local_target_path = upd::get_local_path(root_path, relative_path, working_path);
    auto target_desc = output_files_by_path.find(local_target_path);
    if (target_desc == output_files_by_path.end()) {
      throw unknown_target_error(relative_path);
    }
    update_file_recursively(log_cache, hash_cache, root_path, output_files_by_path, *target_desc, local_depfile_path);
  }

//  update_file_recursively(log_cache, hash_cache, root_path, output_files_by_path, "dist/tests", local_depfile_path);

  std::cout << "done" << std::endl;
  log_cache.close();
  update_log::rewrite_file(log_file_path, temp_log_file_path, log_cache.records());

}

int run_with_options(const cli::options& cli_opts) {
  try {
    if (cli_opts.action != cli::action::update && !cli_opts.relative_target_paths.empty()) {
      cli::fatal_error(std::cerr, cli_opts.color_diagnostics) << "this operation doesn't accept target arguments" << std::endl;
      return 2;
    }
    if (cli_opts.action == cli::action::version) {
      std::cout << "upd v0.1" << std::endl;
      return 0;
    }
    if (cli_opts.action == cli::action::help) {
      cli::print_help();
      return 0;
    }
    auto working_path = io::getcwd_string();
    auto root_path = io::find_root_path(working_path);
    if (cli_opts.action == cli::action::root) {
      std::cout << root_path << std::endl;
      return 0;
    }
    compile_itself(root_path, working_path, cli_opts.action == cli::action::dot_graph, cli_opts.relative_target_paths);
    return 0;
  } catch (io::cannot_find_updfile_error) {
    cli::fatal_error(std::cerr, cli_opts.color_diagnostics) << "cannot find Updfile in the current directory or in any of the parent directories" << std::endl;
    return 2;
  } catch (io::ifstream_failed_error error) {
    cli::fatal_error(std::cerr, cli_opts.color_diagnostics) << "failed to read file `" << error.file_path << "`" << std::endl;
    return 2;
  } catch (update_log::corruption_error) {
    cli::fatal_error(std::cerr, cli_opts.color_diagnostics) << "update log is corrupted; delete or revert the `.upd/log` file" << std::endl;
    return 2;
  } catch (unknown_target_error error) {
    cli::fatal_error(std::cerr, cli_opts.color_diagnostics) << "unknown output file: " << error.relative_path << std::endl;
    return 2;
  } catch (relative_path_out_of_root_error error) {
    cli::fatal_error(std::cerr, cli_opts.color_diagnostics) << "encountered a path out of the project root: " << error.relative_path << std::endl;
    return 2;
  }
}

int run(int argc, char *argv[]) {
  try {
    auto cli_opts = cli::parse_options(argc, argv);
    return run_with_options(cli_opts);
  } catch (cli::unexpected_argument_error error) {
    std::cerr << "upd: fatal: invalid argument: `" << error.arg << "`" << std::endl;
    return 1;
  } catch (cli::incompatible_options_error error) {
    std::cerr << "upd: fatal: options `" << error.first_option
      << "` and `" << error.last_option << "` are in conflict" << std::endl;
    return 1;
  }
}

}

int main(int argc, char *argv[]) {
  return upd::run(argc, argv);
}
