#pragma once
#include "../glpp/shader.h"
#include "../glpp/Program.h"
#include "resource.h"

namespace ds {

glpp::shader loadAndCompileShader(const resource& resource, GLenum shaderType);

glpp::Program loadAndLinkProgram(
  const resource& vertexShader,
  const resource& fragmentShader
);

}
