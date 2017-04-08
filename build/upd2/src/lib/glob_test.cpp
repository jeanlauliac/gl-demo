#include "glob_test.h"

static bool is_segment_empty(upd::glob::segment& target) {
  return
    target.literal.empty() &&
    target.prefix == upd::glob::placeholder::none;
}

upd::glob::pattern parse(const std::string& str_pattern) {
  upd::glob::pattern result;
  upd::glob::segment current;
  for (size_t i = 0; i < str_pattern.size(); ++i) {
    if (str_pattern[i] == '*') {
      if (!is_segment_empty(current)) {
        result.push_back(std::move(current));
        current.literal.clear();
      }
      current.prefix = upd::glob::placeholder::wildcard;
      continue;
    }
    if (str_pattern[i] == '\\') ++i;
    current.literal += str_pattern[i];
  }
  if (!is_segment_empty(current)) {
    result.push_back(std::move(current));
  }
  return result;
}
