#pragma once
#include "../glpp/shader.h"
#include "../glpp/program.h"
#include "resource.h"

namespace ds {

glpp::shader load_and_compile_shader(
  const resource& resource,
  GLenum shaderType
);

glpp::program load_and_link_program(
  const resource& vertexShader,
  const resource& fragmentShader
);

}
