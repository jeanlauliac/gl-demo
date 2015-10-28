#include <stdexcept>
#include "Window.h"

namespace glfwpp {

Window::Window() {
  handle_ = glfwCreateWindow(640, 480, "Simple example", NULL, NULL);
  if (!handle_) {
    throw std::runtime_error("failed to create window");
  }
}

Window::~Window() {
  glfwDestroyWindow(handle_);
}

int Window::shouldClose() {
  return glfwWindowShouldClose(handle_);
}

void Window::swapBuffers() {
  glfwSwapBuffers(handle_);
}

}
