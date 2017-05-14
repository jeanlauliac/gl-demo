#include "window.h"
#include <stdexcept>

namespace glfwpp {

window::window(int width, int height, const char *title, GLFWmonitor *monitor, GLFWwindow *share) {
  handle_ = glfwCreateWindow(width, height, title, monitor, share);
  if (!handle_) {
    throw std::runtime_error("failed to create window");
  }
}

window::window(const window&& window) {
  handle_ = window.handle_;
}

window::~window() {
  glfwDestroyWindow(handle_);
}

void window::get_framebuffer_size(int* width, int* height) const {
  glfwGetFramebufferSize(handle_, width, height);
}

int window::should_close() const {
  return glfwWindowShouldClose(handle_);
}

void window::swap_buffers() {
  glfwSwapBuffers(handle_);
}

}
