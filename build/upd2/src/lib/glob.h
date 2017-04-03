#pragma once

#include "io.h"
#include <iostream>
#include <memory>
#include <queue>
#include <vector>

namespace upd {
namespace glob {

/**
 * A pattern is composed of literals separated by wildcards. For example the
 * pattern "foo_*.cpp" is represented as the vector { "foo_", ".cpp" }.
 */
typedef std::vector<std::string> pattern;

bool match(const pattern& target, const std::string& candidate);

}
}
