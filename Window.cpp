#include <stdexcept>
#include "Window.h"

Window::Window() {
  handle_ = glfwCreateWindow(640, 480, "Simple example", NULL, NULL);
  if (!handle_) {
    throw std::runtime_error("failed to create window");
  }
}

Window::~Window() {
  glfwDestroyWindow(handle_);
}
