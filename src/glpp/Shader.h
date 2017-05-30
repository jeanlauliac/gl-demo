#pragma once
#include "../opengl.h"

namespace glpp {

class shader {
public:
  shader(GLenum shaderType);
  shader(const shader&& shader);
  ~shader();
  shader(shader&) = delete;
  void source(GLsizei count, const GLchar **string, const GLint *length);
  void compile();
  void get_shaderiv(GLenum pname, GLint *params);
  void get_info_log(GLsizei maxLength, GLsizei* length, GLchar* infoLog);
  GLuint handle() const {
    return handle_;
  }

private:
  GLuint handle_;
};

}
