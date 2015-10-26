#pragma once
#include <GLFW/glfw3.h>
#include "Shader.h"

namespace glpp {

class Program {
public:
  Program();
  Program(const Program&& shader);
  ~Program();
  void attachShader(const Shader& shader);
  void link();
  void use();
private:
  Program(Shader&);
  GLuint handle_;
};

}
