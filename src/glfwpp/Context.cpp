#include "context.h"
#include <stdexcept>

namespace glfwpp {

context::context() {
  if (!glfwInit()) {
    throw std::runtime_error("cannot initialize GLFW");
  }
}

context::~context() {
  glfwTerminate();
}

}
