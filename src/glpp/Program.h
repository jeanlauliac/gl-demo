#pragma once
#include "../opengl.h"
#include "shader.h"

namespace glpp {

class program {
public:
  program();
  program(const program&&);
  ~program();
  program(program&) = delete;
  void attachShader(const shader& shader);
  GLint getAttribLocation(const GLchar* name);
  void getProgramiv(GLenum pname, GLint* params);
  GLint getUniformLocation(const GLchar* name);
  GLuint handle() const {
    return handle_;
  }
  void link();
  void use();

private:
  GLuint handle_;
};

}
