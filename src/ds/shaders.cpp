#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include "SystemException.h"
#include "shaders.h"

namespace ds {

glpp::Shader loadAndCompileShader(const resource& resource, GLenum shaderType) {
  glpp::Shader shader(shaderType);
  const char* cstr = reinterpret_cast<const char*>(resource.data);
  shader.source(1, &cstr, nullptr);
  shader.compile();
  GLint status;
  shader.getShaderiv(GL_COMPILE_STATUS, &status);
  if (!status) {
    std::ostringstream errorMessage;
    errorMessage << resource.file_path
      << ": shader compilation failed:" << std::endl;
    GLint logLength;
    shader.getShaderiv(GL_INFO_LOG_LENGTH, &logLength);
    std::vector<char> log(logLength);
    shader.getInfoLog(log.size(), nullptr, &log[0]);
    errorMessage << &log[0] << std::endl;
    throw ds::SystemException(errorMessage.str());
  }
  return shader;
}

glpp::Program loadAndLinkProgram(
  const resource& vertexShader,
  const resource& fragmentShader
) {
  glpp::Program program;
  program.attachShader(
    loadAndCompileShader(vertexShader, GL_VERTEX_SHADER)
  );
  program.attachShader(
    loadAndCompileShader(fragmentShader, GL_FRAGMENT_SHADER)
  );
  program.link();
  GLint status;
  program.getProgramiv(GL_LINK_STATUS, &status);
  if (!status) {
    throw std::runtime_error("shader program linking failed");
  }
  return program;
}

}
