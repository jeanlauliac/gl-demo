#include "inspect.h"

namespace upd {

std::string inspect(unsigned int value, const inspect_options& options) {
  std::ostringstream stream;
  stream << value;
  return stream.str();
}

std::string inspect(std::string value, const inspect_options& options) {
  std::ostringstream stream;
  stream << '"' << value << '"';
  return stream.str();
}

}
