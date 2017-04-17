#include "lib/cli.h"
#include "lib/command_line_template.h"
#include "lib/depfile.h"
#include "lib/inspect.h"
#include "lib/io.h"
#include "lib/json/Lexer.h"
#include "lib/manifest.h"
#include "lib/path.h"
#include "lib/path_glob.h"
#include "lib/substitution.h"
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
#include <unordered_set>
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
  size_t command_line_ix;
  std::vector<std::string> local_input_file_paths;
};

typedef std::unordered_map<std::string, output_file> output_files_by_path_t;

struct update_map {
  output_files_by_path_t output_files_by_path;
  std::vector<command_line_template> command_line_templates;
};

struct unknown_target_error {
  unknown_target_error(const std::string& relative_path):
    relative_path(relative_path) {}
  std::string relative_path;
};

/**
 * At any point during the update, the plan describes the work left to do.
 */
struct update_plan {
  /**
   * The paths of all the output files that are ready to be updated immediately.
   * These files' dependencies either have already been updated, or they are
   * source files written manually.
   */
  std::queue<std::string> queued_output_file_paths;
  /**
   * All the files that remain to update.
   */
  std::unordered_set<std::string> pending_output_file_paths;
  /**
   * For each output file path, indicates how many input files still need to be
   * updated before the output file can be updated.
   */
  std::unordered_map<std::string, int> pending_input_counts_by_path;
  /**
   * For each input file path, indicates what files could potentially be
   * updated after the input file is updated.
   */
  std::unordered_map<std::string, std::vector<std::string>> descendants_by_path;
};

void build_update_plan(
  update_plan& plan,
  const std::unordered_map<std::string, output_file>& output_files_by_path,
  const std::pair<std::string, output_file>& target_descriptor
) {
  auto local_target_path = target_descriptor.first;
  auto pending = plan.pending_output_file_paths.find(local_target_path);
  if (pending != plan.pending_output_file_paths.end()) {
    return;
  }
  plan.pending_output_file_paths.insert(local_target_path);
  int input_count = 0;
  for (auto const& local_input_path: target_descriptor.second.local_input_file_paths) {
    auto input_descriptor = output_files_by_path.find(local_input_path);
    if (input_descriptor == output_files_by_path.end()) {
      continue;
    }
    input_count++;
    plan.descendants_by_path[local_input_path].push_back(local_target_path);
    build_update_plan(plan, output_files_by_path, *input_descriptor);
  }
  if (input_count == 0) {
    plan.queued_output_file_paths.push(local_target_path);
  } else {
    plan.pending_input_counts_by_path[local_target_path] = input_count;
  }
}

void execute_update_plan(
  update_log::cache& log_cache,
  file_hash_cache& hash_cache,
  const std::string& root_path,
  const update_map& updm,
  update_plan& plan,
  const std::string& local_depfile_path
) {
  while (!plan.queued_output_file_paths.empty()) {
    auto local_target_path = plan.queued_output_file_paths.front();
    plan.queued_output_file_paths.pop();
    auto target_descriptor = *updm.output_files_by_path.find(local_target_path);
    auto target_file = target_descriptor.second;
    auto const& command_line_tpl = updm.command_line_templates[target_file.command_line_ix];
    update_file(
      log_cache,
      hash_cache,
      root_path,
      command_line_tpl,
      target_file.local_input_file_paths,
      local_target_path,
      local_depfile_path
    );
    plan.pending_output_file_paths.erase(local_target_path);
    auto descendants_iter = plan.descendants_by_path.find(local_target_path);
    if (descendants_iter == plan.descendants_by_path.end()) {
      continue;
    }
    for (auto const& descendant_path: descendants_iter->second) {
      auto count_iter = plan.pending_input_counts_by_path.find(descendant_path);
      if (count_iter == plan.pending_input_counts_by_path.end()) {
        throw std::runtime_error("update plan is corrupted");
      }
      int& input_count = count_iter->second;
      --input_count;
      if (input_count == 0) {
        plan.queued_output_file_paths.push(descendant_path);
      }
    }
  }
}

/**
 * Should be refactored as it shares plenty with `execute_update_plan`.
 */
template <typename OStream>
void output_dot_graph(
  OStream& os,
  const update_map& updm,
  update_plan& plan
) {
  os << "# generated with `upd --dot-graph`" << std::endl
    << "digraph upd {" << std::endl
    << "  rankdir=\"LR\";" << std::endl;
  while (!plan.queued_output_file_paths.empty()) {
    auto local_target_path = plan.queued_output_file_paths.front();
    plan.queued_output_file_paths.pop();
    auto target_descriptor = *updm.output_files_by_path.find(local_target_path);

    auto const& target_file = target_descriptor.second;
    auto const& command_line_tpl = updm.command_line_templates[target_file.command_line_ix];
    for (auto const& input_path: target_file.local_input_file_paths) {
      os << "  \"" << input_path << "\" -> \""
        << local_target_path << "\" [label=\""
        << command_line_tpl.binary_path << "\"];" << std::endl;
    }

    plan.pending_output_file_paths.erase(local_target_path);
    auto descendants_iter = plan.descendants_by_path.find(local_target_path);
    if (descendants_iter == plan.descendants_by_path.end()) {
      continue;
    }
    for (auto const& descendant_path: descendants_iter->second) {
      auto count_iter = plan.pending_input_counts_by_path.find(descendant_path);
      if (count_iter == plan.pending_input_counts_by_path.end()) {
        throw std::runtime_error("update plan is corrupted");
      }
      int& input_count = count_iter->second;
      --input_count;
      if (input_count == 0) {
        plan.queued_output_file_paths.push(descendant_path);
      }
    }
  }
  os << "}" << std::endl;
}

enum class update_rule_input_type { source, rule };

struct update_rule_input {
  static update_rule_input from_source(size_t ix) {
    return {
      .type = update_rule_input_type::source,
      .input_ix = ix,
    };
  }

  static update_rule_input from_rule(size_t ix) {
    return {
      .type = update_rule_input_type::rule,
      .input_ix = ix,
    };
  }

  update_rule_input_type type;
  size_t input_ix;
};

struct update_rule {
  size_t command_line_ix;
  update_rule_input input;
  substitution::pattern output;
};

update_map get_update_map(const std::string& root_path) {
  update_map result;

  std::vector<std::string> local_obj_file_paths;
  std::vector<std::string> local_test_cpp_file_basenames;
  std::vector<std::string> local_test_cppt_file_paths;

  result.command_line_templates = {
    get_cppt_command_line(root_path),
    get_compile_command_line(src_file_type::cpp),
    get_compile_command_line(src_file_type::c),
    get_index_tests_command_line(),
    get_link_command_line(),
  };

  std::vector<path_glob::pattern> patterns = {
    path_glob::parse("(src/lib/**/*).cppt"),
    path_glob::parse("(src/lib/**/*).cpp"),
    path_glob::parse("(src/lib/**/*).c"),
  };

  std::vector<std::vector<captured_string>> matches(patterns.size());
  path_glob::matcher<io::dir_files_reader> cppt_matcher(root_path, patterns);
  path_glob::match cppt_match;
  while (cppt_matcher.next(cppt_match)) {
    matches[cppt_match.pattern_ix].push_back({
      .value = std::move(cppt_match.local_path),
      .captured_groups = std::move(cppt_match.captured_groups),
    });
  }

  std::vector<update_rule> rules = {
    {
      .command_line_ix = 0,
      .input = update_rule_input::from_source(0),
      .output = substitution::parse("dist/($1).cpp"),
    },
  };

  std::vector<std::vector<captured_string>> rule_captured_paths(rules.size());
  // TODO: find correct rule order

  for (size_t i = 0; i < rules.size(); ++i) {
    const auto& rule = rules[i];
    const auto& input_captures =
      rule.input.type == update_rule_input_type::source
      ? matches[rule.input.input_ix]
      : rule_captured_paths[rule.input.input_ix];
    std::unordered_map<std::string, std::pair<std::vector<std::string>, std::vector<size_t>>> data_by_path;
    for (const auto& input_capture: input_captures) {
      auto local_output = substitution::resolve(rule.output.segments, input_capture);
      auto& datum = data_by_path[local_output.value];
      datum.first.push_back(input_capture.value);
      datum.second = local_output.segment_start_ids;
    }
    auto& captured_paths = rule_captured_paths[i];
    captured_paths.resize(data_by_path.size());
    size_t k = 0;
    for (const auto& input: data_by_path) {
      if (result.output_files_by_path.count(input.first)) {
        throw std::runtime_error("two rules with same outputs");
      }
      result.output_files_by_path[input.first] = {
        .command_line_ix = rule.command_line_ix,
        .local_input_file_paths = input.second.first,
      };
      captured_paths[k] = substitution::capture(
        rule.output.capture_groups,
        input.first,
        input.second.second
      );
      ++k;
    }
  }

  for (const auto& match: matches[0]) {
    local_test_cpp_file_basenames.push_back(match.get_sub_string(0));
    local_test_cppt_file_paths.push_back(match.value);
  }

  for (const auto& match: matches[1]) {
    auto local_obj_path = "dist/" + match.get_sub_string(0) + ".o";
    result.output_files_by_path[local_obj_path] = {
      .command_line_ix = 1,
      .local_input_file_paths = { match.value }
    };
    local_obj_file_paths.push_back(local_obj_path);
  }

  for (const auto& match: matches[2]) {
    auto local_obj_path = "dist/" + match.get_sub_string(0) + ".o";
    result.output_files_by_path[local_obj_path] = {
      .command_line_ix = 2,
      .local_input_file_paths = { match.value }
    };
    local_obj_file_paths.push_back(local_obj_path);
  }

  result.output_files_by_path["dist/tests.cpp"] = {
    .command_line_ix = 3,
    .local_input_file_paths = { local_test_cppt_file_paths }
  };
  local_test_cpp_file_basenames.push_back("tests");

  auto local_upd_object_file_paths = local_obj_file_paths;
  result.output_files_by_path["dist/src/main.o"] = {
    .command_line_ix = 1,
    .local_input_file_paths = { "src/main.cpp" }
  };
  local_upd_object_file_paths.push_back("dist/src/main.o");

  auto local_test_object_file_paths = local_obj_file_paths;
  result.output_files_by_path["dist/tools/lib/testing.o"] = {
    .command_line_ix = 1,
    .local_input_file_paths = { "tools/lib/testing.cpp" }
  };
  local_test_object_file_paths.push_back("dist/tools/lib/testing.o");

  for (auto const& basename: local_test_cpp_file_basenames) {
    auto local_path = "dist/" + basename + ".cpp";
    auto local_obj_path = "dist/" + basename + ".o";
    auto pcli = get_compile_command_line(src_file_type::cpp);
    result.output_files_by_path[local_obj_path] = {
      .command_line_ix = 1,
      .local_input_file_paths = { local_path }
    };
    local_test_object_file_paths.push_back(local_obj_path);
  }

  result.output_files_by_path["dist/upd"] = {
    .command_line_ix = 4,
    .local_input_file_paths = { local_upd_object_file_paths }
  };
  result.output_files_by_path["dist/tests"] = {
    .command_line_ix = 4,
    .local_input_file_paths = { local_test_object_file_paths }
  };

  return result;
}

struct no_targets_error {};

void compile_itself(
  const std::string& root_path,
  const std::string& working_path,
  bool print_graph,
  bool update_all_files,
  const std::vector<std::string>& relative_target_paths
) {
  const update_map updm = get_update_map(root_path);
  const auto& output_files_by_path = updm.output_files_by_path;
  update_plan plan;

  std::vector<std::string> local_target_paths;
  for (auto const& relative_path: relative_target_paths) {
    auto local_target_path = upd::get_local_path(root_path, relative_path, working_path);
    auto target_desc = output_files_by_path.find(local_target_path);
    if (target_desc == output_files_by_path.end()) {
      throw unknown_target_error(relative_path);
    }
    build_update_plan(plan, output_files_by_path, *target_desc);
  }
  if (update_all_files) {
    for (auto const& target_desc: output_files_by_path) {
      build_update_plan(plan, output_files_by_path, target_desc);
    }
  }
  if (plan.pending_output_file_paths.size() == 0) {
    throw no_targets_error();
  }
  if (print_graph) {
    output_dot_graph(std::cout, updm, plan);
    return;
  }

  std::string log_file_path = root_path + "/" + CACHE_FOLDER + "/log";
  std::string temp_log_file_path = root_path + "/" + CACHE_FOLDER + "/log_rewritten";
  update_log::cache log_cache = update_log::cache::from_log_file(log_file_path);
  file_hash_cache hash_cache;
  auto local_depfile_path = CACHE_FOLDER + "/depfile";
  auto depfile_path = root_path + '/' + local_depfile_path;
  if (mkfifo(depfile_path.c_str(), 0644) != 0 && errno != EEXIST) {
    throw std::runtime_error("cannot make depfile FIFO");
  }

  execute_update_plan(log_cache, hash_cache, root_path, updm, plan, local_depfile_path);

  std::cout << "done" << std::endl;
  log_cache.close();
  update_log::rewrite_file(log_file_path, temp_log_file_path, log_cache.records());

}

int run_with_options(const cli::options& cli_opts) {
  try {
    if (!(
      cli_opts.action == cli::action::update ||
      cli_opts.action == cli::action::dot_graph
    )) {
      if (!cli_opts.relative_target_paths.empty()) {
        cli::fatal_error(std::cerr, cli_opts.color_diagnostics) << "this operation doesn't accept target arguments" << std::endl;
        return 2;
      }
      if (cli_opts.update_all_files) {
        cli::fatal_error(std::cerr, cli_opts.color_diagnostics) << "this operation doesn't accept `--all`" << std::endl;
        return 2;
      }
    }
    if (cli_opts.update_all_files && !cli_opts.relative_target_paths.empty()) {
      cli::fatal_error(std::cerr, cli_opts.color_diagnostics) << "cannot have both explicit targets and `--all`" << std::endl;
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
    compile_itself(root_path, working_path, cli_opts.action == cli::action::dot_graph, cli_opts.update_all_files, cli_opts.relative_target_paths);
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
  } catch (no_targets_error error) {
    cli::fatal_error(std::cerr, cli_opts.color_diagnostics) << "specify at least one target to update" << std::endl;
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
