#pragma once

#include "captured_string.h"
#include <string>
#include <vector>

namespace upd {
namespace substitution {

struct segment {
  std::string literal;
  bool has_captured_group;
  size_t captured_group_ix;
};

struct pattern {
  std::vector<segment> segments;
  std::vector<std::pair<size_t, size_t>> capture_groups;
};

struct resolved {
  std::string value;
  std::vector<size_t> segment_start_ids;
};

resolved resolve(
  const std::vector<segment>& segments,
  const captured_string& input
);

captured_string capture(
  const std::vector<std::pair<size_t, size_t>>& capture_groups,
  const std::string& resolved_string,
  const std::vector<size_t>& resolved_start_segment_ids
);

}
}
