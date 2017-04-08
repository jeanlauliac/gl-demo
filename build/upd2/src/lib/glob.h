#pragma once

#include "io.h"
#include <iostream>
#include <memory>
#include <queue>
#include <vector>

namespace upd {
namespace glob {

enum class placeholder {
  none,
  wildcard
};

struct segment {
  segment(): prefix(placeholder::none) {}
  segment(placeholder prefix): prefix(prefix) {}
  segment(const std::string& literal):
    prefix(placeholder::none), literal(literal) {}
  segment(placeholder prefix, const std::string& literal):
    prefix(prefix), literal(literal) {}
  segment(const segment& target):
    prefix(target.prefix), literal(target.literal) {}
  segment(segment&& target):
    prefix(target.prefix), literal(std::move(target.literal)) {}

  placeholder prefix;
  std::string literal;
};

/**
 * A pattern is composed of literals separated by wildcards. For example the
 * pattern "foo_*.cpp" is represented as the vector { "foo_", ".cpp" }.
 */
typedef std::vector<segment> pattern;

bool match(const pattern& target, const std::string& candidate);

}
}
