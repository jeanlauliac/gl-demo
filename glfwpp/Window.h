#pragma once
#include <GLFW/glfw3.h>

namespace glfwpp {

struct window {
  window(int width, int height, const char *title, GLFWmonitor *monitor, GLFWwindow *share);
  window(const window&& window);
  window(window&) = delete;
  ~window();

  void get_framebuffer_size(int* width, int* height) const;
  int should_close() const;
  void swap_buffers();
  GLFWwindow* handle() const {
    return handle_;
  }

private:
  GLFWwindow* handle_;
};

}
