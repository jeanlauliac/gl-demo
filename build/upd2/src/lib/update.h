#pragma once

#include "update_log.h"
#include "xxhash64.h"
#include <future>
#include <unistd.h>

namespace upd {

XXH64_hash_t hash_command_line(const command_line& command_line);

XXH64_hash_t hash_files(
  file_hash_cache& hash_cache,
  const std::string& root_path,
  const std::vector<std::string>& local_paths
);

XXH64_hash_t get_target_imprint(
  file_hash_cache& hash_cache,
  const std::string& root_path,
  const std::vector<std::string>& local_src_paths,
  std::vector<std::string> dependency_local_paths,
  const command_line& command_line
);

bool is_file_up_to_date(
  update_log::cache& log_cache,
  file_hash_cache& hash_cache,
  const std::string& root_path,
  const std::string& local_target_path,
  const std::vector<std::string>& local_src_paths,
  const command_line& command_line
);

void run_command_line(const std::string& root_path, command_line target);

void update_file(
  update_log::cache& log_cache,
  file_hash_cache& hash_cache,
  const std::string& root_path,
  const command_line_template& param_cli,
  const std::vector<std::string>& local_src_paths,
  const std::string& local_target_path,
  const std::string& local_depfile_path,
  bool print_commands
);

}
