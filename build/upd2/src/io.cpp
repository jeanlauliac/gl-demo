#include "io.h"
#include <libgen.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>

namespace upd {
namespace io {

const char* UPDFILE_SUFFIX = "/Updfile";

std::string getcwd_string() {
  char temp[MAXPATHLEN];
  if (getcwd(temp, MAXPATHLEN) == nullptr) {
    throw std::runtime_error("unable to get current working directory");
  }
  return temp;
}

std::string dirname_string(const std::string& path) {
  if (path.size() >= MAXPATHLEN) {
    throw std::runtime_error("string too long");
  }
  char temp[MAXPATHLEN];
  strcpy(temp, path.c_str());
  return dirname(temp);
}

static bool is_regular_file(const std::string& path) {
  struct stat data;
  auto stat_ret = stat(path.c_str(), &data);
  if (stat_ret != 0) {
    if (errno == ENOENT) {
      return false;
    }
    throw std::runtime_error("`stat` function returned unhanled error");
  }
  return S_ISREG(data.st_mode) != 0;
}

std::string find_root_path() {
  std::string path = getcwd_string();
  bool found = is_regular_file(path + UPDFILE_SUFFIX);
  while (!found && path != "/") {
    path = dirname_string(path);
    found = is_regular_file(path + UPDFILE_SUFFIX);
  }
  if (!found) throw cannot_find_updfile_error();
  return path;
}

dir::dir(const std::string& path): ptr_(opendir(path.c_str())) {
  if (ptr_ == nullptr) throw std::runtime_error("opendir() failed");
}

dir::~dir() {
  closedir(ptr_);
}

void dir::open(const std::string& path) {
  closedir(ptr_);
  ptr_ = opendir(path.c_str());
  if (ptr_ == nullptr) throw std::runtime_error("opendir() failed");
}

dir_files_reader::dir_files_reader(const std::string& path): target_(path) {}

struct dirent* dir_files_reader::next() {
  return readdir(target_.ptr());
}

void dir_files_reader::open(const std::string& path) {
  target_.open(path);
}

}
}
