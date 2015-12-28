#include <stdexcept>
#include "Window.h"

namespace glfwpp {

Window::Window(int width, int height, const char *title, GLFWmonitor *monitor, GLFWwindow *share) {
  handle_ = glfwCreateWindow(width, height, title, monitor, share);
  if (!handle_) {
    throw std::runtime_error("failed to create window");
  }
}

Window::Window(const Window&& window) {
  handle_ = window.handle_;
}

Window::~Window() {
  glfwDestroyWindow(handle_);
}

void Window::getFramebufferSize(int* width, int* height) {
  glfwGetFramebufferSize(handle_, width, height);
}

int Window::shouldClose() {
  return glfwWindowShouldClose(handle_);
}

void Window::swapBuffers() {
  glfwSwapBuffers(handle_);
}

}
