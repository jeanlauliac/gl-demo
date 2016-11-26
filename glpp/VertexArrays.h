#pragma once
#include <GLFW/glfw3.h>

namespace glpp {

template <int TCount>
class VertexArrays {
public:
  VertexArrays() {
    glGenVertexArrays(TCount, handles_);
  }
  VertexArrays(const VertexArrays&& other) {
    handles_ = other.handles_;
  }
  ~VertexArrays() {
    glDeleteVertexArrays(TCount, handles_);
  }
  const GLuint* handles() const {
    return handles_;
  }
private:
  VertexArrays(VertexArrays&);
  GLuint handles_[TCount];
};

}
