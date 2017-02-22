#pragma once

#include "xxhash.h"

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

XXH64_hash_t hash_file(unsigned long long seed, const std::string file_path);

}
