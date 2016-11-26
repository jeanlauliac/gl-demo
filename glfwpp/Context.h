#pragma once
#include <GLFW/glfw3.h>
#include "Window.h"

namespace glfwpp {

class Context {
public:
  Context();
  ~Context();
  Context(Context&&);

  void windowHint(int target, int value) {
    glfwWindowHint(target, value);
  }

  void makeContextCurrent(const Window& window) {
    glfwMakeContextCurrent(window.handle());
  }

  GLFWkeyfun setKeyCallback(const Window& window, GLFWkeyfun cbfun) {
    return glfwSetKeyCallback(window.handle(), cbfun);
  }

private:
  Context(Context&);
};

}
