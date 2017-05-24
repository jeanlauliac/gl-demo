#include "program.h"

namespace glpp {

program::program() {
  handle_ = glCreateProgram();
}

program::program(const program&& other) {
  handle_ = other.handle_;
}

program::~program() {
  glDeleteProgram(handle_);
}

void program::attachShader(const shader& target) {
  glAttachShader(handle_, target.handle());
}

GLint program::getAttribLocation(const GLchar* name) {
  return glGetAttribLocation(handle_, name);
}

void program::getProgramiv(GLenum pname, GLint* params) {
  glGetProgramiv(handle_, pname, params);
}

GLint program::getUniformLocation(const GLchar* name) {
  return glGetUniformLocation(handle_, name);
}

void program::link() {
  glLinkProgram(handle_);
}

void program::use() {
  glUseProgram(handle_);
}

}
