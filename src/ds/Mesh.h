#pragma once
#include <glm/glm.hpp>

namespace ds {

struct Vertex {
  glm::vec3 position;
  glm::vec3 normal;
};

struct Mesh {
  std::vector<Vertex> vertices;
  std::vector<glm::uvec3> triangles;
};

}
