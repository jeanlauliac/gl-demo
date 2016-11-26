#include <stdexcept>
#include "Context.h"

namespace glfwpp {

Context::Context() {
  if (!glfwInit()) {
    throw std::runtime_error("cannot initialize GLFW");
  }
}

Context::~Context() {
  glfwTerminate();
}

Context::Context(Context&&) {
}

}
