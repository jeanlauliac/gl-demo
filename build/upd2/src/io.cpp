#include "io.h"
#include <cstdlib>
#include <fstream>
#include <libgen.h>
#include <map>
#include <sstream>
#include <stdlib.h>
#include <sys/param.h>
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

std::string find_root_path() {
  std::string path = getcwd_string();
  std::ifstream updfile(path + UPDFILE_SUFFIX);
  while (!updfile.is_open() && path != "/") {
    path = dirname_string(path);
    updfile.open(path + UPDFILE_SUFFIX);
  }
  if (!updfile.is_open()) {
    throw cannot_find_updfile_error();
  }
  return path;
}

}
}
