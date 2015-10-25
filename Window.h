#include <GLFW/glfw3.h>

class Window {
public:
  Window();
  ~Window();
  GLFWwindow* handle() const {
    return handle_;
  }
private:
  Window(const Window&);
  GLFWwindow* handle_;
};
