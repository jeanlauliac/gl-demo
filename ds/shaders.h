#pragma once
#include "../glpp/Shader.h"
#include "../glpp/Program.h"

namespace ds {

glpp::Shader loadAndCompileShader(std::string filePath, GLenum shaderType);

glpp::Program loadAndLinkProgram(
  std::string vertexShaderPath,
  std::string fragmentShaderPath
);

}
