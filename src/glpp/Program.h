#pragma once
#include "../opengl.h"
#include "shader.h"

namespace glpp {

class Program {
public:
  Program();
  Program(const Program&& shader);
  ~Program();
  Program(Program&) = delete;
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
