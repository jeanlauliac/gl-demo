#pragma once
#include <stddef.h>

namespace ds {

/**
 * An arbitrary dataset embeded into the binary file.
 */
struct resource {
  /**
   * The original file path this resource has been build from. It may be
   * a `nullptr` for production builds.
   */
  const char* file_path;
  /**
   * The data in raw form. A zero/nullptr is appended at the very end, so that
   * the data can be readily used as a C string without copy.
   */
  const unsigned char* data;
  /**
   * Length of the data in bytes. This does not include the terminating
   * zero/nullptr.
   */
  const size_t size;
};

}
