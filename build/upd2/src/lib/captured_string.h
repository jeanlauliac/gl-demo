#pragma once

#include <string>
#include <vector>

namespace upd {

struct no_such_captured_group_error {};

struct captured_string {
  std::string get_sub_string(size_t index) const {
    if (index >= captured_groups.size()) throw no_such_captured_group_error();
    const auto& group = captured_groups[index];
    return value.substr(group.first, group.second - group.first);
  }

  typedef std::pair<size_t, size_t> group;
  typedef std::vector<group> groups;
  std::string value;
  groups captured_groups;
};

}
