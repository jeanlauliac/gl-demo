#pragma once
#include <GLFW/glfw3.h>

namespace glfwpp {

class Window {
public:
  Window();
  ~Window();
  GLFWwindow* handle() const {
    return handle_;
  }
private:
  Window(Window&);
  GLFWwindow* handle_;
};

}
