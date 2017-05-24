#pragma once
#include "../opengl.h"

namespace glpp {

class Shader {
public:
  Shader(GLenum shaderType);
  Shader(const Shader&& shader);
  ~Shader();
  void source(GLsizei count, const GLchar **string, const GLint *length);
  void compile();
  void getShaderiv(GLenum pname, GLint *params);
  void getInfoLog(GLsizei maxLength, GLsizei* length, GLchar* infoLog);
  GLuint handle() const {
    return handle_;
  }
private:
  Shader(Shader&);
  GLuint handle_;
};

}
