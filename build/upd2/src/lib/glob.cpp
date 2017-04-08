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
  size_t segment_ix = 0, candidate_ix = 0, bookmark_ix = 0;
  size_t last_wildcard_segment_ix = 0;
  bool has_bookmark = false;
  if (target[segment_ix].prefix != placeholder::wildcard) goto process_literal;

process_wildcard:
  bookmark_ix = candidate_ix;
  last_wildcard_segment_ix = segment_ix;
  has_bookmark = true;

process_literal:
  if (!match_literal(target[segment_ix].literal, candidate, candidate_ix)) {
    goto restore_wildcard;
  }
  ++segment_ix;
  if (segment_ix == target.size()) {
    if (candidate_ix == candidate.size()) return true;
    goto restore_wildcard;
  }
  if (target[segment_ix].prefix == placeholder::wildcard)
    goto process_wildcard;
  goto process_literal;

restore_wildcard:
  if (!has_bookmark) return false;
  ++bookmark_ix;
  candidate_ix = bookmark_ix;
  segment_ix = last_wildcard_segment_ix;
  if (candidate_ix + target[segment_ix].literal.size() > candidate.size())
    return false;
  goto process_literal;
}

}
}
