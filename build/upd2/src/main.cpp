#include "lib/cli.h"
#include "lib/command_line_template.h"
#include "lib/depfile.h"
#include "lib/inspect.h"
#include "lib/io.h"
#include "lib/istream_char_reader.h"
#include "lib/json/lexer.h"
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

struct output_file {
  size_t command_line_ix;
  std::vector<std::string> local_input_file_paths;
};

typedef std::unordered_map<std::string, output_file> output_files_by_path_t;

struct update_map {
  output_files_by_path_t output_files_by_path;
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
  std::vector<command_line_template> command_line_templates,
  const std::string& local_depfile_path
) {
  while (!plan.queued_output_file_paths.empty()) {
    auto local_target_path = plan.queued_output_file_paths.front();
    plan.queued_output_file_paths.pop();
    auto target_descriptor = *updm.output_files_by_path.find(local_target_path);
    auto target_file = target_descriptor.second;
    auto const& command_line_tpl = command_line_templates[target_file.command_line_ix];
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
  update_plan& plan,
  std::vector<command_line_template> command_line_templates
) {
  os << "# generated with `upd --dot-graph`" << std::endl
    << "digraph upd {" << std::endl
    << "  rankdir=\"LR\";" << std::endl;
  while (!plan.queued_output_file_paths.empty()) {
    auto local_target_path = plan.queued_output_file_paths.front();
    plan.queued_output_file_paths.pop();
    auto target_descriptor = *updm.output_files_by_path.find(local_target_path);

    auto const& target_file = target_descriptor.second;
    auto const& command_line_tpl = command_line_templates[target_file.command_line_ix];
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

struct cannot_refer_to_later_rule_error {};

struct update_manifest {
  std::vector<command_line_template> command_line_templates;
  std::vector<path_glob::pattern> source_patterns;
  std::vector<manifest::update_rule> rules;
};

std::vector<std::vector<captured_string>>
crawl_source_patterns(
  const std::string& root_path,
  const std::vector<path_glob::pattern>& patterns
) {
  std::vector<std::vector<captured_string>> matches(patterns.size());
  path_glob::matcher<io::dir_files_reader> matcher(root_path, patterns);
  path_glob::match match;
  while (matcher.next(match)) {
    matches[match.pattern_ix].push_back({
      .value = std::move(match.local_path),
      .captured_groups = std::move(match.captured_groups),
    });
  }
  return matches;
}

update_map get_update_map(
  const std::string& root_path,
  const update_manifest& manifest
) {
  update_map result;
  auto matches = crawl_source_patterns(root_path, manifest.source_patterns);
  std::vector<std::vector<captured_string>> rule_captured_paths(manifest.rules.size());
  for (size_t i = 0; i < manifest.rules.size(); ++i) {
    const auto& rule = manifest.rules[i];
    std::unordered_map<std::string, std::pair<std::vector<std::string>, std::vector<size_t>>> data_by_path;
    for (const auto& input: rule.inputs) {
      if (input.type == manifest::update_rule_input_type::rule) {
        if (input.input_ix >= i) {
          throw cannot_refer_to_later_rule_error();
        }
      }
      const auto& input_captures =
        input.type == manifest::update_rule_input_type::source
        ? matches[input.input_ix]
        : rule_captured_paths[input.input_ix];
      for (const auto& input_capture: input_captures) {
        auto local_output = substitution::resolve(rule.output.segments, input_capture);
        auto& datum = data_by_path[local_output.value];
        datum.first.push_back(input_capture.value);
        datum.second = local_output.segment_start_ids;
      }
    }
    auto& captured_paths = rule_captured_paths[i];
    captured_paths.resize(data_by_path.size());
    size_t k = 0;
    for (const auto& datum: data_by_path) {
      if (result.output_files_by_path.count(datum.first)) {
        throw std::runtime_error("two rules with same outputs");
      }
      result.output_files_by_path[datum.first] = {
        .command_line_ix = rule.command_line_ix,
        .local_input_file_paths = datum.second.first,
      };
      captured_paths[k] = substitution::capture(
        rule.output.capture_groups,
        datum.first,
        datum.second.second
      );
      ++k;
    }
  }
  return result;
}

struct no_targets_error {};

manifest::manifest read_manifest(const std::string& root_path) {
  std::ifstream file;
  file.exceptions(std::ifstream::badbit);
  file.open(root_path + io::UPDFILE_SUFFIX);
  istream_char_reader<std::ifstream> reader(file);
  json::lexer<istream_char_reader<std::ifstream>> lexer(reader);
  return manifest::parse(lexer);
}

update_manifest get_manifest(const std::string& root_path) {
  update_manifest result;

  auto manifest_content = read_manifest(root_path);

  result.source_patterns = manifest_content.source_patterns;
  result.rules = manifest_content.rules;
  result.command_line_templates = manifest_content.command_line_templates;

  return result;
}

void compile_itself(
  const std::string& root_path,
  const std::string& working_path,
  bool print_graph,
  bool update_all_files,
  const std::vector<std::string>& relative_target_paths
) {
  auto manifest = get_manifest(root_path);
  const update_map updm = get_update_map(root_path, manifest);
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
    output_dot_graph(std::cout, updm, plan, manifest.command_line_templates);
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

  execute_update_plan(log_cache, hash_cache, root_path, updm, plan, manifest.command_line_templates, local_depfile_path);

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
    cli::fatal_error(std::cerr, cli_opts.color_diagnostics) << "cannot find updfile.json in the current directory or in any of the parent directories" << std::endl;
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
