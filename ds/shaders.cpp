#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include "shaders.h"

namespace ds {

glpp::Shader loadAndCompileShader(std::string filePath, GLenum shaderType) {
  std::ifstream ifs(filePath);
  std::string str(
    (std::istreambuf_iterator<char>(ifs)),
    std::istreambuf_iterator<char>()
  );

  glpp::Shader shader(shaderType);
  const char* cstr = str.c_str();
  shader.source(1, &cstr, nullptr);
  shader.compile();
  GLint status;
  shader.getShaderiv(GL_COMPILE_STATUS, &status);
  if (!status) {
    std::ostringstream errorMessage;
    errorMessage << filePath << ": shader compilation failed:" << std::endl;
    GLint logLength;
    shader.getShaderiv(GL_INFO_LOG_LENGTH, &logLength);
    std::vector<char> log(logLength);
    shader.getInfoLog(log.size(), nullptr, &log[0]);
    errorMessage << &log[0] << std::endl;
    throw std::runtime_error(errorMessage.str());
  }
  return shader;
}

glpp::Program loadAndLinkProgram(
  std::string vertexShaderPath,
  std::string fragmentShaderPath
) {
  glpp::Program program;
  program.attachShader(
    loadAndCompileShader(vertexShaderPath, GL_VERTEX_SHADER)
  );
  program.attachShader(
    loadAndCompileShader(fragmentShaderPath, GL_FRAGMENT_SHADER)
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
