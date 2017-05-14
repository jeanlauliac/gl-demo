#pragma once
#include "window.h"
#include <GLFW/glfw3.h>

namespace glfwpp {

struct context {
  context();
  ~context();
  context(context&) = delete;
  context(context&&) {}

  void window_hint(int target, int value) {
    glfwWindowHint(target, value);
  }

  void make_context_current(const window& window) {
    glfwMakeContextCurrent(window.handle());
  }

  GLFWkeyfun set_key_callback(const window& window, GLFWkeyfun cbfun) {
    return glfwSetKeyCallback(window.handle(), cbfun);
  }
};

}
