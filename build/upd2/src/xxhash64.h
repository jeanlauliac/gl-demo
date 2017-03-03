#pragma once

#include "xxhash.h"
#include <unordered_map>

namespace upd {

/**
 * Helper for using xxHash in a streaming fashion,
 * that takes care of the state automatically.
 */
struct xxhash64 {
  xxhash64(unsigned long long seed):
    state_(XXH64_createState(), XXH64_freeState) { reset(seed); }
  void reset(unsigned long long seed) { XXH64_reset(state_.get(), seed); }
  void update(const void* input, size_t length) {
    XXH64_update(state_.get(), input, length);
  }
  XXH64_hash_t digest() { return XXH64_digest(state_.get()); }
private:
  std::unique_ptr<XXH64_state_t, XXH_errorcode(*)(XXH64_state_t*)> state_;
};

/**
 * Hashes an entire file, fast. Since the hash will be different for
 * small changes, this is a handy way to check if a source file changed
 * since a previous update.
 */
XXH64_hash_t hash_file(unsigned long long seed, const std::string file_path);

/**
 * Many source files, such as C++ headers, have an impact on the compilation of
 * multiple object files at a time. So it's handy to cache the hashes for these
 * source files.
 */
struct file_hash_cache {
  unsigned long long hash(const std::string& file_path);
private:
  std::unordered_map<std::string, unsigned long long> cache_;
};

}
