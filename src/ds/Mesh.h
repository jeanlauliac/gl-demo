#pragma once
#include <glm/glm.hpp>
#include <vector>

namespace ds {

struct vertex {
  glm::vec3 position;
  glm::vec3 normal;
};

struct mesh {
  std::vector<vertex> vertices;
  std::vector<glm::uvec3> triangles;
};

}
