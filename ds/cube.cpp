#include <vector>
#include "cube.h"

namespace ds {

Mesh getCube() {
  std::vector<Vertex> vertices;
  std::vector<glm::uvec3> triangles;
  for (int dimension = 0; dimension < 3; ++dimension) {
    for (int direction = -1; direction <= 1; direction += 2) {
      auto base = vertices.size();
      glm::vec3 faceNormal;
      faceNormal[dimension] = static_cast<float>(direction);
      for (int sideways = -1; sideways <= 1; sideways += 2) {
        for (int vertically = -1; vertically <= 1; vertically += 2) {
          glm::vec3 position;
          position[dimension] = static_cast<float>(direction) * 0.5f;
          position[(dimension + 1) % 3] = static_cast<float>(sideways) * 0.5f;
          position[(dimension + 2) % 3] = static_cast<float>(vertically) * 0.5f;
          Vertex vertex = {.normal = faceNormal, .position = position};
          vertices.push_back(vertex);
        }
      }
      triangles.push_back(glm::uvec3(base, base + 1, base + 2));
      triangles.push_back(glm::uvec3(base + 1, base + 2, base + 3));
    }
  }
  return {.vertices = vertices, .triangles = triangles};
}

}
