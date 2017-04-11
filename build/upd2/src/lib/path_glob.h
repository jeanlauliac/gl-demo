#pragma once

#include "glob.h"
#include "io.h"
#include <iostream>
#include <memory>
#include <unordered_map>
#include <vector>

namespace upd {
namespace path_glob {

enum class capture_point_type { wildcard, ent_name };

struct capture_point {
  capture_point_type type;
  size_t ent_name_segment_ix;
  size_t segment_ix;
};

inline bool operator==(const capture_point& left, const capture_point& right) {
  return
    left.type == right.type &&
    left.segment_ix == right.segment_ix && (
      left.type == capture_point_type::wildcard ||
      left.ent_name_segment_ix == right.ent_name_segment_ix
    );
}

struct capture_group {
  capture_point from;
  capture_point to;
};

inline bool operator==(const capture_group& left, const capture_group& right) {
  return
    left.from == right.from &&
    left.to == right.to;
}

struct segment {
  void clear() {
    ent_name.clear();
    has_wildcard = false;
  }

  glob::pattern ent_name;
  bool has_wildcard;
};

inline bool operator==(const segment& left, const segment& right) {
  return
    left.has_wildcard == right.has_wildcard &&
    left.ent_name == right.ent_name;
}

/**
 * The pattern to match a full file path, represented as each of its successive
 * entity names. For example
 */
struct pattern {
  std::vector<capture_group> capture_groups;
  std::vector<segment> segments;
};

inline bool operator==(const pattern& left, const pattern& right) {
  return
    left.segments == right.segments &&
    left.capture_groups == right.capture_groups;
}

struct match {
  std::string local_path;
};

template <typename DirFilesReader>
struct matcher {
  matcher(const std::string& root_path, const pattern& pattern):
    root_path_(root_path),
    pattern_(pattern),
    pending_dirs_({ { "", { 0 } } }) {};

  bool next(match& next_match) {
    auto ent = next_ent_();
    while (ent != nullptr) {
      std::string name = ent->d_name;
      const auto& segments = pattern_.segments;
      // replace loop by a function "next_segment_"
      for (auto segment_ix: segment_ixs_) {
        const auto& name_pattern = segments[segment_ix].ent_name;
        if (segments[segment_ix].has_wildcard) {
          if (ent->d_type == DT_DIR) {
            pending_dirs_[path_prefix_ + '/' + name].push_back(segment_ix);
          }
        }
        if (glob::match(name_pattern, name)) {
          if (ent->d_type == DT_DIR && segment_ix + 1 < segments.size()) {
            pending_dirs_[path_prefix_ + '/' + name].push_back(segment_ix + 1);
          }
          if (ent->d_type == DT_REG && segment_ix == segments.size() - 1) {
            next_match.local_path = path_prefix_.substr(1) + '/' + name;
            return true;
          }
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
    const auto& next_dir_iter = pending_dirs_.cbegin();
    if (next_dir_iter == pending_dirs_.cend()) {
      return false;
    }
    path_prefix_ = next_dir_iter->first;
    segment_ixs_ = next_dir_iter->second;
    pending_dirs_.erase(next_dir_iter);
    dir_reader_.open(root_path_ + path_prefix_);
    return true;
  }

  struct pending_dir {
    std::string path_prefix;
    size_t ent_name_pattern_ix;
  };

  std::string root_path_;
  pattern pattern_;
  DirFilesReader dir_reader_;
  std::unordered_map<std::string, std::vector<size_t>> pending_dirs_;
  std::string path_prefix_;
  std::vector<size_t> segment_ixs_;
};

}
}
