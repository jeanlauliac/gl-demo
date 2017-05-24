#include "Program.h"

namespace glpp {

Program::Program() {
  handle_ = glCreateProgram();
}

Program::Program(const Program&& program) {
  handle_ = program.handle_;
}

Program::~Program() {
  glDeleteProgram(handle_);
}

void Program::attachShader(const shader& target) {
  glAttachShader(handle_, target.handle());
}

GLint Program::getAttribLocation(const GLchar* name) {
  return glGetAttribLocation(handle_, name);
}

void Program::getProgramiv(GLenum pname, GLint* params) {
  glGetProgramiv(handle_, pname, params);
}

GLint Program::getUniformLocation(const GLchar* name) {
  return glGetUniformLocation(handle_, name);
}

void Program::link() {
  glLinkProgram(handle_);
}

void Program::use() {
  glUseProgram(handle_);
}

}
