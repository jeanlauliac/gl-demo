#pragma once
#include "../glpp/Shader.h"
#include "../glpp/Program.h"
#include "resource.h"

namespace ds {

glpp::Shader loadAndCompileShader(const resource& resource, GLenum shaderType);

glpp::Program loadAndLinkProgram(
  const resource& vertexShader,
  const resource& fragmentShader
);

}
