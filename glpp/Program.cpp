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

void Program::attachShader(const Shader& shader) {
  glAttachShader(handle_, shader.handle());
}

void Program::link() {
  glLinkProgram(handle_);
}

void Program::use() {
  glUseProgram(handle_);
}

}
