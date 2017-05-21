#pragma once
#include <GLFW/glfw3.h>

namespace glpp {

template <int TCount>
class Buffers {
public:
  Buffers() {
    glGenBuffers(TCount, handles_);
  }
  Buffers(const Buffers&& other) {
    handles_ = other.handles_;
  }
  ~Buffers() {
    glDeleteBuffers(TCount, handles_);
  }
  const GLuint* handles() const {
    return handles_;
  }
private:
  Buffers(Buffers&);
  GLuint handles_[TCount];
};

}
