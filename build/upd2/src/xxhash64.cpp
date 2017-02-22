#include "io.h"
#include "xxhash64.h"
#include <array>
#include <fstream>

namespace upd {

XXH64_hash_t hash_file(unsigned long long seed, const std::string file_path) {
  upd::xxhash64 hash(seed);
  std::ifstream ifs(file_path, std::ifstream::binary);
  std::array<char, 1024> buffer;
  do {
    ifs.read(buffer.data(), buffer.size());
    hash.update(buffer.data(), ifs.gcount());
  } while (ifs.good());
  if (ifs.bad()) {
    throw io::ifstream_failed_error(file_path);
  }
  return hash.digest();
}

}
