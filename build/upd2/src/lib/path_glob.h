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
  size_t segment_ix;
  capture_point_type type;
  size_t ent_name_segment_ix;
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

enum class invalid_pattern_string_reason {
  duplicate_directory_wildcard,
  duplicate_wildcard,
  escape_char_at_end,
  unexpected_capture_close,
};

struct invalid_pattern_string_error {
  invalid_pattern_string_error(invalid_pattern_string_reason reason):
    reason(reason) {};

  invalid_pattern_string_reason reason;
};

pattern parse(const std::string& pattern_string);

struct match {
  std::string get_captured_string(size_t index) const {
    const auto& group = captured_groups[index];
    return local_path.substr(group.first, group.second - group.first);
  }

  size_t pattern_ix;
  std::string local_path;
  std::vector<std::pair<size_t, size_t>> captured_groups;
};

template <typename DirFilesReader>
struct matcher {
private:
  struct bookmark {
    size_t pattern_ix;
    size_t segment_ix;
    std::unordered_map<size_t, size_t> captured_from_ids;
    std::unordered_map<size_t, size_t> captured_to_ids;
  };
  typedef std::unordered_map<std::string, std::vector<bookmark>>
    pending_dirs_type;

public:
  matcher(const std::string& root_path, const std::vector<pattern>& patterns):
    root_path_(root_path),
    patterns_(patterns),
    pending_dirs_(generate_initial_pending_dirs_(patterns)),
    bookmark_ix_(0),
    ent_(nullptr) {}

  matcher(const std::string& root_path, const pattern& single_pattern):
    matcher(root_path, std::vector<pattern>({ single_pattern })) {}

  bool next(match& next_match) {
    while (next_bookmark_()) {
      std::string name = ent_->d_name;
      const auto& bookmark = bookmarks_[bookmark_ix_];
      const auto& segments = patterns_[bookmark.pattern_ix].segments;
      auto segment_ix = bookmark.segment_ix;
      const auto& name_pattern = segments[segment_ix].ent_name;
      if (segments[segment_ix].has_wildcard && ent_->d_type == DT_DIR) {
        push_wildcard_match_(name, bookmark);
      }
      std::vector<size_t> indices;
      if (!glob::match(name_pattern, name, indices)) continue;
      if (ent_->d_type == DT_DIR && segment_ix + 1 < segments.size()) {
        push_ent_name_match_(name, bookmark, indices);
      }
      if (ent_->d_type == DT_REG && segment_ix == segments.size() - 1) {
        finalize_match_(next_match, name, bookmark, indices);
        return true;
      }
    }
    return false;
  }

private:
  static pending_dirs_type generate_initial_pending_dirs_(
    const std::vector<pattern>& patterns
  ) {
    std::vector<bookmark> initial_bookmarks;
    for (size_t i = 0; i < patterns.size(); ++i) {
      initial_bookmarks.push_back({
        .pattern_ix = i,
        .segment_ix = 0,
      });
    }
    return pending_dirs_type({ { "/", std::move(initial_bookmarks) } });
  }

  void push_wildcard_match_(const std::string& name, const bookmark& target) {
    auto captured_from_ids = target.captured_from_ids;
    auto captured_to_ids = target.captured_to_ids;
    const auto& pattern_ = patterns_[target.pattern_ix];
    for (size_t i = 0; i < pattern_.capture_groups.size(); ++i) {
      const auto& group = pattern_.capture_groups[i];
      if (
        group.from.type == capture_point_type::wildcard &&
        group.from.segment_ix == target.segment_ix &&
        captured_from_ids.count(i) == 0
      ) {
        captured_from_ids[i] = path_prefix_.size();
      }
    }
    pending_dirs_[path_prefix_ + name + '/'].push_back({
      .segment_ix = target.segment_ix,
      .captured_from_ids = std::move(captured_from_ids),
      .captured_to_ids = std::move(captured_to_ids),
      .pattern_ix = target.pattern_ix,
    });
  }

  void push_ent_name_match_(
    const std::string& name,
    const bookmark& target,
    const std::vector<size_t> match_indices
  ) {
    auto captured_from_ids = target.captured_from_ids;
    auto captured_to_ids = target.captured_to_ids;
    update_captures_for_ent_name_(
      target,
      match_indices,
      captured_from_ids,
      captured_to_ids
    );
    pending_dirs_[path_prefix_ + name + '/'].push_back({
      .segment_ix = target.segment_ix + 1,
      .captured_from_ids = std::move(captured_from_ids),
      .captured_to_ids = std::move(captured_to_ids),
      .pattern_ix = target.pattern_ix,
    });
  }

  void finalize_match_(
    match& next_match,
    const std::string& name,
    const bookmark& target,
    const std::vector<size_t> match_indices
  ) {
    auto captured_from_ids = target.captured_from_ids;
    auto captured_to_ids = target.captured_to_ids;
    update_captures_for_ent_name_(
      target,
      match_indices,
      captured_from_ids,
      captured_to_ids
    );
    next_match.local_path = path_prefix_.substr(1) + name;
    const auto& pattern_ = patterns_[target.pattern_ix];
    next_match.captured_groups.resize(pattern_.capture_groups.size());
    next_match.pattern_ix = target.pattern_ix;
    for (size_t i = 0; i < pattern_.capture_groups.size(); ++i) {
      next_match.captured_groups[i] = {
        captured_from_ids.at(i) - 1,
        captured_to_ids.at(i) - 1,
      };
    }
  }

  void update_captures_for_ent_name_(
    const bookmark& target,
    const std::vector<size_t> match_indices,
    std::unordered_map<size_t, size_t>& captured_from_ids,
    std::unordered_map<size_t, size_t>& captured_to_ids
  ) {
    const auto& pattern_ = patterns_[target.pattern_ix];
    for (size_t i = 0; i < pattern_.capture_groups.size(); ++i) {
      const auto& group = pattern_.capture_groups[i];
      if (
        group.from.type == capture_point_type::ent_name &&
        group.from.segment_ix == target.segment_ix
      ) {
        auto ent_name_ix = match_indices[group.from.ent_name_segment_ix];
        captured_from_ids[i] = path_prefix_.size() + ent_name_ix;
      }
      if (
        group.to.type == capture_point_type::ent_name &&
        group.to.segment_ix == target.segment_ix
      ) {
        auto ent_name_ix = match_indices[group.to.ent_name_segment_ix];
        captured_to_ids[i] = path_prefix_.size() + ent_name_ix;
      }
    }
  }

  bool next_bookmark_() {
    ++bookmark_ix_;
    if (bookmark_ix_ < bookmarks_.size()) return true;
    bookmark_ix_ = 0;
    if (!next_ent_()) return false;
    while (ent_->d_name[0] == '.') {
      if (!next_ent_()) return false;
    }
    return true;
  }

  bool next_ent_() {
    if (!dir_reader_.is_open()) {
      if (!next_dir_()) return false;
    }
    ent_ = dir_reader_.next();
    while (ent_ == nullptr) {
      if (!next_dir_()) {
        dir_reader_.close();
        return false;
      }
      ent_ = dir_reader_.next();
    }
    return true;
  }

  bool next_dir_() {
    const auto& next_dir_iter = pending_dirs_.cbegin();
    if (next_dir_iter == pending_dirs_.cend()) {
      return false;
    }
    path_prefix_ = next_dir_iter->first;
    bookmarks_ = next_dir_iter->second;
    pending_dirs_.erase(next_dir_iter);
    dir_reader_.open(root_path_ + path_prefix_);
    return true;
  }

  std::string root_path_;
  std::vector<pattern> patterns_;
  DirFilesReader dir_reader_;
  pending_dirs_type pending_dirs_;
  std::string path_prefix_;
  std::vector<bookmark> bookmarks_;
  size_t bookmark_ix_;
  dirent* ent_;
};

}
}
