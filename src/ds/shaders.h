#pragma once
#include "../glpp/shader.h"
#include "../glpp/program.h"
#include "resource.h"

namespace ds {

glpp::shader load_and_compile_shader(
  const resource& resource,
  GLenum shader_type
);

glpp::program load_and_link_program(
  const resource& vertex_shader,
  const resource& fragment_shader
);

}
