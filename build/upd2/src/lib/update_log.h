#pragma once

#include <fstream>
#include <iostream>
#include <unordered_map>
#include <vector>

namespace upd {
namespace update_log {

struct corruption_error {};

struct file_record {
  /**
   * This is a hash digest of the command and the all the source files and that
   * dependencies that generated that particular file.
   */
  unsigned long long imprint;
  /**
   * This is a hash digest of the file content. This is handy to know if a file
   * hasn't been corrupted after having been generated.
   */
  unsigned long long hash;
  /**
   * This is all the files on which this particular file depends on for
   * generation, in addition to its direct sources. For example, in C/C++
   * an object file depends on the c/cpp file, but headers are additional
   * dependencies that we must consider.
   */
  std::vector<std::string> dependency_local_paths;
};

enum class record_mode { append, truncate };

struct recorder {
  recorder(const std::string& file_path, record_mode mode);
  void record(const std::string& local_file_path, const file_record& record);
  void close();

private:
  std::ofstream log_file_;
};

typedef std::unordered_map<std::string, file_record> records_by_file;

/**
 * We keep of copy of the update log in memory. New additions are written right
 * away to the log so that even if the process crashes we keep track of
 * what files are already updated.
 */
struct cache {
  cache(const std::string& file_path, const records_by_file& cached_records):
    recorder_(file_path, record_mode::append), cached_records_(cached_records) {}
  records_by_file::iterator find(const std::string& local_file_path);
  records_by_file::iterator end();
  void record(const std::string& local_file_path, const file_record& record);
  void close() { recorder_.close(); }
  const records_by_file& records() const { return cached_records_; }
  static records_by_file records_from_log_file(const std::string& log_file_path);
  static cache from_log_file(const std::string& log_file_path);

private:
  recorder recorder_;
  records_by_file cached_records_;
};

struct failed_to_rewrite_error {};

/**
 * Once all the updates has happened, we have appended the new entries to the
 * update log, but there may be duplicates. To finalize the log, we rewrite
 * it from scratch, and we replace the existing log. `rename` is normally an
 * atomic operation, so we ensure no data is lost.
 */
void rewrite_file(
  const std::string& file_path,
  const std::string& temporary_file_path,
  const records_by_file& records
);

}
}
