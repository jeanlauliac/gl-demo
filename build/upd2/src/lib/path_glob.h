#pragma once

#include "glob.h"
#include "io.h"
#include <iostream>
#include <memory>
#include <queue>
#include <vector>

namespace upd {
namespace path_glob {

/**
 * The pattern to match a full file path, represented as each of its successive
 * entity names. For example
 */
struct pattern {
  std::vector<glob::pattern> ent_name_patterns;
};

struct match {
  std::string local_path;
};

template <typename DirFilesReader>
struct matcher {
  matcher(const std::string& root_path, const pattern& pattern):
    root_path_(root_path),
    pattern_(pattern),
    pending_dirs_({ { .path_prefix = "", .ent_name_pattern_ix = 0 } }) {};

  bool next(match& next_match) {
    auto ent = next_ent_();
    while (ent != nullptr) {
      std::string name = ent->d_name;
      const auto& name_pts = pattern_.ent_name_patterns;
      const auto& name_ix = current_dir_.ent_name_pattern_ix;
      const auto& name_pattern = name_pts[name_ix];
      if (glob::match(name_pattern, name)) {
        if (ent->d_type == DT_DIR && name_ix + 1 < name_pts.size()) {
          pending_dirs_.push({
            .path_prefix = current_dir_.path_prefix + '/' + name,
            .ent_name_pattern_ix = name_ix + 1,
          });
        }
        if (ent->d_type == DT_REG && name_ix == name_pts.size() - 1) {
          next_match.local_path =
            current_dir_.path_prefix.substr(1) + '/' + name;
          return true;
        }
      }
      ent = next_ent_();
    }
    return false;
  }

private:
  dirent* next_ent_() {
    if (!dir_reader_.is_open()) {
      if (!open_next_dir_()) return nullptr;
    }
    auto ent = dir_reader_.next();
    while (ent == nullptr) {
      if (!open_next_dir_()) {
        dir_reader_.close();
        return nullptr;
      }
      ent = dir_reader_.next();
    }
    return ent;
  }

  bool open_next_dir_() {
    if (pending_dirs_.empty()) {
      return false;
    }
    const auto& next_dir = pending_dirs_.front();
    pending_dirs_.pop();
    dir_reader_.open(root_path_ + next_dir.path_prefix);
    current_dir_ = next_dir;
    return true;
  }

  struct pending_dir {
    std::string path_prefix;
    size_t ent_name_pattern_ix;
  };

  std::string root_path_;
  pattern pattern_;
  DirFilesReader dir_reader_;
  std::queue<pending_dir> pending_dirs_;
  pending_dir current_dir_;
};

}
}
