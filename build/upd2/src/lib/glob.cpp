#include "glob.h"

namespace upd {
namespace glob {

struct matcher {
  matcher(const pattern& target, const std::string& candidate):
    target(target),
    candidate(candidate),
    segment_ix(0),
    candidate_ix(0),
    bookmark_ix(0),
    last_wildcard_segment_ix(0),
    has_bookmark(false) {}

  bool operator()() {
    start_new_segment();
    while (true) {
      if (!match_literal(target[segment_ix].literal)) {
        if (!restore_wildcard()) return false;
        continue;
      }
      ++segment_ix;
      if (segment_ix == target.size()) {
        if (candidate_ix == candidate.size()) return true;
        if (!restore_wildcard()) return false;
        continue;
      }
      start_new_segment();
    }
  }

  bool match_literal(const std::string& literal) {
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

  bool restore_wildcard() {
    if (!has_bookmark) return false;
    ++bookmark_ix;
    candidate_ix = bookmark_ix;
    segment_ix = last_wildcard_segment_ix;
    if (candidate_ix + target[segment_ix].literal.size() > candidate.size())
      return false;
    return true;
  }

  void start_wildcard() {
    bookmark_ix = candidate_ix;
    last_wildcard_segment_ix = segment_ix;
    has_bookmark = true;
  }

  void start_new_segment() {
    if (target[segment_ix].prefix == placeholder::wildcard)
      start_wildcard();
  }

  const pattern& target;
  const std::string& candidate;
  size_t segment_ix;
  size_t candidate_ix;
  size_t bookmark_ix;
  size_t last_wildcard_segment_ix;
  bool has_bookmark;
};

bool match(const pattern& target, const std::string& candidate) {
  return matcher(target, candidate)();
}

}
}
