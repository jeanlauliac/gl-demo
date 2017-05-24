#include "shader.h"

namespace glpp {

shader::shader(GLenum shaderType) {
  handle_ = glCreateShader(shaderType);
}

shader::shader(const shader&& shader) {
  handle_ = shader.handle_;
}

shader::~shader() {
  glDeleteShader(handle_);
}

void shader::source(GLsizei count, const GLchar **string, const GLint *length) {
  glShaderSource(handle_, count, string, length);
}

void shader::compile() {
  glCompileShader(handle_);
}

void shader::getShaderiv(GLenum pname, GLint *params) {
  glGetShaderiv(handle_, pname, params);
}

void shader::getInfoLog(GLsizei maxLength, GLsizei* length, GLchar* infoLog) {
  glGetShaderInfoLog(handle_, maxLength, length, infoLog);
}

}
