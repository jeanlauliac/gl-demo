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
  void attach_shader(const shader& shader);
  GLint get_attrib_location(const GLchar* name);
  void get_programiv(GLenum pname, GLint* params);
  GLint get_uniform_location(const GLchar* name);
  GLuint handle() const {
    return handle_;
  }
  void link();
  void use();

private:
  GLuint handle_;
};

}
