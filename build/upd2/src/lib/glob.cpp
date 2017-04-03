#include "glob.h"

namespace upd {
namespace glob {

bool match_literal(
  const std::string& literal,
  const std::string& candidate,
  size_t& candidate_ix
) {
  size_t literal_ix = 0;
  while (
    candidate_ix < candidate.size() &&
    literal_ix < literal.size() &&
    candidate[candidate_ix] == literal[literal_ix]
  ) {
    ++candidate_ix;
    ++literal_ix;
  }
  return literal_ix == literal.size();
}

bool match(const pattern& target, const std::string& candidate) {
 if (target.size() == 0) {
    return false;
  }
  size_t target_ix = 0, candidate_ix = 0, bookmark_ix = 0;
  bool has_bookmark = false;
  while (target_ix < target.size()) {
    const auto& literal = target[target_ix];
    bool literal_matched = match_literal(literal, candidate, candidate_ix);
    bool candidate_matched = candidate_ix == candidate.size();
    bool has_more_wildcards = target_ix < target.size() - 1;
    if (literal_matched && (candidate_matched || has_more_wildcards)) {
      ++target_ix;
      bookmark_ix = candidate_ix;
      has_bookmark = true;
      continue;
    }
    if (!has_bookmark || bookmark_ix == candidate.size()) return false;
    ++bookmark_ix;
    candidate_ix = bookmark_ix;
    if (candidate.size() - candidate_ix < literal.size()) return false;
  }
  return true;
}

}
}
