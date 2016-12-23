#include <cstdlib>
#include <fstream>
#include <iostream>
#include <libgen.h>
#include <stdlib.h>
#include <sys/param.h>
#include <unistd.h>

void compile_itself(const std::string& root_path) {
  auto ret = system((
    std::string("clang++ -c -o ") + root_path + "/dist/main.o " +
    "-Wall -std=c++14 -fcolor-diagnostics " +
    "-I /usr/local/include " + root_path + "/src/main.cpp"
  ).c_str());
  if (ret != 0) {
    throw "compile failed";
  }
  ret = system((
    std::string("clang++ -o ") + root_path + "/dist/upd " +
    "-Wall -std=c++14 -fcolor-diagnostics -L /usr/local/lib " +
    root_path + "/dist/main.o"
  ).c_str());
  if (ret != 0) {
    throw "link failed";
  }
}

std::string getcwd_string() {
  char temp[MAXPATHLEN];
  if (getcwd(temp, MAXPATHLEN) == nullptr) {
    throw std::runtime_error("unable to get current working directory");
  }
  return temp;
}

const char* UPDFILE_BASENAME = "/Updfile";

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
  std::ifstream updfile(path + UPDFILE_BASENAME);
  while (!updfile.is_open() && path != "/") {
    path = dirname_string(path);
    updfile.open(path + UPDFILE_BASENAME);
  }
  if (!updfile.is_open()) {
    throw std::runtime_error("unable to find Updfile directory");
  }
  return path;
}

int main() {
  auto root_path = find_root_path();
  compile_itself(root_path);
  std::cout << root_path << std::endl;
}
