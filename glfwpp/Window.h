#pragma once
#include <GLFW/glfw3.h>

namespace glfwpp {

class Window {
public:
  Window(int width, int height, const char *title, GLFWmonitor *monitor, GLFWwindow *share);
  Window(const Window&& window);
  ~Window();
  void getFramebufferSize(int* width, int* height);
  int shouldClose();
  void swapBuffers();
  GLFWwindow* handle() const {
    return handle_;
  }
private:
  Window(Window&);
  GLFWwindow* handle_;
};

}
