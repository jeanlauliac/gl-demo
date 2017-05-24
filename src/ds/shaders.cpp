#include "shaders.h"
#include "system_error.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

namespace ds {

glpp::shader load_and_compile_shader(const resource& resource, GLenum shaderType) {
  glpp::shader result(shaderType);
  const char* cstr = reinterpret_cast<const char*>(resource.data);
  result.source(1, &cstr, nullptr);
  result.compile();
  GLint status;
  result.getShaderiv(GL_COMPILE_STATUS, &status);
  if (!status) {
    std::ostringstream errorMessage;
    errorMessage << resource.file_path
      << ": shader compilation failed:" << std::endl;
    GLint logLength;
    result.getShaderiv(GL_INFO_LOG_LENGTH, &logLength);
    std::vector<char> log(logLength);
    result.getInfoLog(log.size(), nullptr, &log[0]);
    errorMessage << &log[0] << std::endl;
    throw ds::system_error(errorMessage.str());
  }
  return result;
}

glpp::program load_and_link_program(
  const resource& vertexShader,
  const resource& fragmentShader
) {
  glpp::program result;
  result.attachShader(
    load_and_compile_shader(vertexShader, GL_VERTEX_SHADER)
  );
  result.attachShader(
    load_and_compile_shader(fragmentShader, GL_FRAGMENT_SHADER)
  );
  result.link();
  GLint status;
  result.getProgramiv(GL_LINK_STATUS, &status);
  if (!status) {
    throw std::runtime_error("shader program linking failed");
  }
  return result;
}

}
