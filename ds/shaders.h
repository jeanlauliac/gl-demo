#pragma once
#include "../glpp/Shader.h"
#include "../glpp/Program.h"
#include "Resource.h"

namespace ds {

glpp::Shader loadAndCompileShader(const Resource& resource, GLenum shaderType);

glpp::Program loadAndLinkProgram(
  const Resource& vertexShader,
  const Resource& fragmentShader
);

}
