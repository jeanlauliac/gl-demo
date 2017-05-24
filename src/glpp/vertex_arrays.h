#pragma once
#include "../opengl.h"

namespace glpp {

template <int TCount>
class vertex_arrays {
public:
  vertex_arrays() {
    glGenVertexArrays(TCount, handles_);
  }
  vertex_arrays(const vertex_arrays&& other) {
    handles_ = other.handles_;
  }
  ~vertex_arrays() {
    glDeleteVertexArrays(TCount, handles_);
  }
  vertex_arrays(vertex_arrays&) = delete;
  const GLuint* handles() const {
    return handles_;
  }

private:
  GLuint handles_[TCount];
};

}
