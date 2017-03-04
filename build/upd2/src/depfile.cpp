#include "depfile.h"

namespace upd {
namespace depfile {

bool parse_token_handler::end() {
  if (!(state_ == state_t::read_dep || state_ == state_t::done)) {
    throw parse_error("unexpected end");
  }
  state_ = state_t::done;
  return false;
}

bool parse_token_handler::colon() {
  if (state_ != state_t::read_colon) {
    throw parse_error("unexpected colon operator");
  }
  state_ = state_t::read_dep;
  return true;
}

bool parse_token_handler::string(const std::string& file_path) {
  if (state_ == state_t::read_target) {
    data_.target_path = file_path;
    state_ = state_t::read_colon;
    return true;
  }
  if (state_ == state_t::read_dep) {
    data_.dependency_paths.push_back(file_path);
    return true;
  }
  throw parse_error("unexpected string `" + file_path + "`");
}

bool parse_token_handler::new_line() {
  if (state_ == state_t::read_target) {
    return true;
  }
  if (state_ != state_t::read_dep) {
    throw parse_error("unexpected newline");
  }
  state_ = state_t::done;
  return true;
}

void read(const std::string& depfile_path, depfile_data& data) {
  std::ifstream depfile;
  depfile.exceptions(std::ifstream::badbit);
  depfile.open(depfile_path);
  auto deps = parse(depfile, data);
}

}
}
