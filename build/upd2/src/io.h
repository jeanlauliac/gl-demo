#pragma once

#include <dirent.h>
#include <iostream>
#include <sys/types.h>

namespace upd {
namespace io {

extern const char* UPDFILE_SUFFIX;

/**
 * Get current working directory but as a `std::string`, easier to deal with.
 */
std::string getcwd_string();

/**
 * Same as `dirname`, but with `std::string`.
 */
std::string dirname_string(const std::string& path);

struct cannot_find_updfile_error {};

/**
 * Figure out the root directory containing the Updfile, from which the relative
 * paths in the manifest are based on.
 */
std::string find_root_path();

struct dir {
  dir(const std::string& path);
  ~dir();
  DIR* ptr() const { return ptr_; }
private:
  dir(dir&);
  DIR* ptr_;
};

struct dir_files_reader {
  dir_files_reader(const std::string& path);
  struct dirent* next();
private:
  dir target_;
};

}
}
