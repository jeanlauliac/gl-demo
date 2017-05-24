#pragma once
#include "../opengl.h"

namespace glpp {

template <int TCount>
class buffers {
public:
  buffers() {
    glGenBuffers(TCount, handles_);
  }
  buffers(const buffers&& other) {
    handles_ = other.handles_;
  }
  ~buffers() {
    glDeleteBuffers(TCount, handles_);
  }
  buffers(buffers&) = delete;
  const GLuint* handles() const {
    return handles_;
  }

private:
  GLuint handles_[TCount];
};

}
