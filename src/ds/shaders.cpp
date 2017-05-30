#include "shaders.h"
#include "system_error.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

namespace ds {

glpp::shader load_and_compile_shader(const resource& resource, GLenum shader_type) {
  glpp::shader result(shader_type);
  const char* cstr = reinterpret_cast<const char*>(resource.data);
  result.source(1, &cstr, nullptr);
  result.compile();
  GLint status;
  result.get_shaderiv(GL_COMPILE_STATUS, &status);
  if (status == GL_TRUE) {
    return result;
  }
  std::ostringstream errorMessage;
  errorMessage << resource.file_path
    << ": shader compilation failed:" << std::endl;
  GLint logLength;
  result.get_shaderiv(GL_INFO_LOG_LENGTH, &logLength);
  std::vector<char> log(logLength);
  result.get_info_log(log.size(), nullptr, &log[0]);
  errorMessage << &log[0] << std::endl;
  throw ds::system_error(errorMessage.str());
}

glpp::program load_and_link_program(
  const resource& vertex_shader,
  const resource& fragment_shader
) {
  glpp::program result;
  result.attach_shader(
    load_and_compile_shader(vertex_shader, GL_VERTEX_SHADER)
  );
  result.attach_shader(
    load_and_compile_shader(fragment_shader, GL_FRAGMENT_SHADER)
  );
  result.link();
  GLint status;
  result.get_programiv(GL_LINK_STATUS, &status);
  if (!status) {
    throw std::runtime_error("shader program linking failed");
  }
  return result;
}

}
