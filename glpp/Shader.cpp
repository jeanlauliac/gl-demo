#include "Shader.h"

namespace glpp {

Shader::Shader(GLenum shaderType) {
  handle_ = glCreateShader(shaderType);
}

Shader::Shader(const Shader&& shader) {
  handle_ = shader.handle_;
}

Shader::~Shader() {
  glDeleteShader(handle_);
}

void Shader::source(GLsizei count, const GLchar **string, const GLint *length) {
  glShaderSource(handle_, count, string, length);
}

void Shader::compile() {
  glCompileShader(handle_);
}

void Shader::getShaderiv(GLenum pname, GLint *params) {
  glGetShaderiv(handle_, pname, params);
}

void Shader::getInfoLog(GLsizei maxLength, GLsizei* length, GLchar* infoLog) {
  glGetShaderInfoLog(handle_, maxLength, length, infoLog);
}

}
