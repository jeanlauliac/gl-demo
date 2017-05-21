#pragma once
#include <GLFW/glfw3.h>
#include "Shader.h"

namespace glpp {

class Program {
public:
  Program();
  Program(const Program&& shader);
  ~Program();
  void attachShader(const Shader& shader);
  GLint getAttribLocation(const GLchar* name);
  void getProgramiv(GLenum pname, GLint* params);
  GLint getUniformLocation(const GLchar* name);
  GLuint handle() const {
    return handle_;
  }
  void link();
  void use();
private:
  Program(Shader&);
  GLuint handle_;
};

}
