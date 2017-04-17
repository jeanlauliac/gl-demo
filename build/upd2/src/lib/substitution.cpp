#include "substitution.h"

namespace upd {
namespace substitution {

resolved resolve(
  const std::vector<segment>& segments,
  const captured_string& input
) {
  resolved result;
  result.segment_start_ids.resize(segments.size());
  for (size_t i = 0; i < segments.size(); ++i) {
    result.segment_start_ids[i] = result.value.size();
    const auto& segment = segments[i];
    result.value += segment.literal;
    if (segment.has_captured_group) {
      result.value += input.get_sub_string(segment.captured_group_ix);
    }
  }
  return result;
}

captured_string capture(
  const std::vector<std::pair<size_t, size_t>>& capture_groups,
  const std::string& resolved_string,
  const std::vector<size_t>& resolved_start_segment_ids
) {
  captured_string result;
  result.value = resolved_string;
  result.captured_groups.resize(capture_groups.size());
  for (size_t j = 0; j < capture_groups.size(); ++j) {
    const auto& capture_group = capture_groups[j];
    result.captured_groups[j].first =
      resolved_start_segment_ids[capture_group.first];
    result.captured_groups[j].second =
      resolved_start_segment_ids[capture_group.second];
  }
  return result;
}

}
}
