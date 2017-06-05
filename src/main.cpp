#include "ds/icosahedron.h"
#include "ds/shaders.h"
#include "ds/system_error.h"
#include "glfwpp/context.h"
#include "glfwpp/window.h"
#include "glpp/buffers.h"
#include "glpp/program.h"
#include "glpp/shader.h"
#include "glpp/vertex_arrays.h"
#include "opengl.h"
#include "resources.h"
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <map>
#include <random>
#include <sstream>
#include <thread>
#include <vector>

static void errorCallback(int error, const char* description)
{
  std::cerr << description << std::endl;
}

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
    glfwSetWindowShouldClose(window, GL_TRUE);
  }
}

enum class window_mode { WINDOW, FULLSCREEN, };

struct options {
  options(): show_help(false), window_mode(window_mode::WINDOW) {}

  bool show_help;
  window_mode window_mode;
};

static options parse_options(int argc, char* argv[]) {
  options result;
  for (++argv, --argc; argc > 0; ++argv, --argc) {
    const auto arg = std::string(*argv);
    if (arg == "--fullscreen" || arg == "-f") {
      result.window_mode = window_mode::FULLSCREEN;
    } else if (arg == "--help" || arg == "-h") {
      result.show_help = true;
    } else {
      throw std::runtime_error("unknown argument: `" + arg + "`");
    }
  }
  return result;
}

static int show_help() {
  std::cout << R"END(Usage: gl-demo [options]
Options:
  --fullscreen, -f          Create a fullscreen window
  --help, -h                Show this
)END";
  return 0;
}

static glfwpp::window create_window(
  glfwpp::context& context,
  window_mode window_mode
) {
  if (window_mode == window_mode::WINDOW) {
    return glfwpp::window(800, 600, "Demo", nullptr, nullptr);
  }
  GLFWmonitor* monitor = glfwGetPrimaryMonitor();
  const GLFWvidmode* mode = glfwGetVideoMode(monitor);
  context.window_hint(GLFW_RED_BITS, mode->redBits);
  context.window_hint(GLFW_GREEN_BITS, mode->greenBits);
  context.window_hint(GLFW_BLUE_BITS, mode->blueBits);
  context.window_hint(GLFW_REFRESH_RATE, mode->refreshRate);
  return glfwpp::window(mode->width, mode->height, "Demo", monitor, nullptr);
}

static glfwpp::context create_context() {
  glfwpp::context context;
  context.window_hint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  context.window_hint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  context.window_hint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  context.window_hint(GLFW_CONTEXT_VERSION_MINOR, 1);
  context.window_hint(GLFW_RESIZABLE, GL_FALSE);
  return context;
}

void enableGlew() {
  glewExperimental = GL_TRUE;
  GLenum err = glewInit();
  if(err != GLEW_OK) {
    throw std::runtime_error(
      std::string("cannot initialize GLEW: ") +
      reinterpret_cast<const char*>(glewGetErrorString(err))
    );
  }
}

glm::mat4 getPerspectiveProjection(const glfwpp::window& window) {
  int width, height;
  window.get_framebuffer_size(&width, &height);
  float ratio = static_cast<float>(width) / static_cast<float>(height);
  return glm::perspective(1.221f, ratio, 0.01f, 100.0f);
}

std::ostream &operator<<(std::ostream &os, const glm::vec3& vec) {
  return os << "[" << vec.x << ", " << vec.y << ", " << vec.z << "]";
}

std::ostream &operator<<(std::ostream &os, const ds::vertex& vertex) {
  return
    os << "CubeVertex {position: " << vertex.position
    << ", normal: " << vertex.normal << "}";
}

template <typename T>
std::ostream &operator<<(std::ostream &os, const std::vector<T>& vector) {
  std::cout << "std::vector {" << std::endl;
  auto first = true;
  for (auto& item : vector) {
    if (!first) {
      std::cout << ", " << std::endl;
    }
    first = false;
    std::cout << "  " << item;
  }
  return std::cout << std::endl << "}";
}

struct ico_sphere_generator {
  ds::mesh operator()() {
    positions_.clear();
    for (const auto& vertex: ds::icosahedron.vertices) {
      positions_.push_back(glm::normalize(vertex.position));
    }
    std::vector<glm::uvec3> triangles = ds::icosahedron.triangles;
    for (size_t pass_ix = 0; pass_ix < 5; ++pass_ix) {
      middle_positions_index_.clear();
      std::vector<glm::uvec3> new_triangles;
      for (const auto& triangle: triangles) {
        auto ix1 = triangle.x;
        auto ix2 = triangle.y;
        auto ix3 = triangle.z;
        auto mid1 = get_middle_position_(ix1, ix2);
        auto mid2 = get_middle_position_(ix2, ix3);
        auto mid3 = get_middle_position_(ix3, ix1);
        new_triangles.push_back({ ix1, mid1, mid3 });
        new_triangles.push_back({ ix2, mid2, mid1 });
        new_triangles.push_back({ ix3, mid3, mid2 });
        new_triangles.push_back({ mid1, mid2, mid3 });
      }
      triangles = std::move(new_triangles);
    }
    ds::mesh result;
    result.triangles = std::move(triangles);
    for (const auto& position: positions_) {
      result.vertices.push_back({
        .position = position,
        .normal = position,
      });
    }
    return result;
  }

private:
  size_t get_middle_position_(size_t first, size_t second) {
    if (first > second) {
      std::swap(first, second);
    }
    std::pair<size_t, size_t> key = {first, second};
    auto iter = middle_positions_index_.find(key);
    if (iter != middle_positions_index_.end()) {
      return iter->second;
    }
    const auto& first_vec = positions_[first];
    const auto& second_vec = positions_[second];
    glm::vec3 mid_vec = glm::normalize((first_vec + second_vec) * 0.5f);
    positions_.push_back(mid_vec);
    auto ix = positions_.size() - 1;
    middle_positions_index_.insert({ key, ix });
    return ix;
  }

  std::vector<glm::vec3> positions_;
  std::map<std::pair<size_t, size_t>, size_t> middle_positions_index_;
};

void mod_altitude(glm::vec3& position, float amount) {
  auto length = glm::length(position);
  position *= (length + amount) / length;
}

float OCEAN_ALTITUDE = 1.0f;
float EPSILON = 0.000001f;

glm::vec3 get_gravity_center(const std::vector<ds::vertex>& vertices) {
  glm::vec3 result;
  size_t weight = 0;
  for (const auto& vertex: vertices) {
    result = (result * static_cast<float>(weight) + vertex.position) /
      static_cast<float>(weight + 1);
    weight++;
  }
  return result;
}

void recenter_vertices(std::vector<ds::vertex>& vertices) {
  auto center = get_gravity_center(vertices);
  for (auto& vertex: vertices) {
    vertex.position -= center;
  }
}

float get_average_altitude(std::vector<ds::vertex>& vertices) {
  float result = 0;
  size_t weight = 0;
  for (const auto& vertex: vertices) {
    result =
      (result * static_cast<float>(weight) + glm::length(vertex.position)) /
      static_cast<float>(weight + 1);
    weight++;
  }
  return result;
}

struct planet {
  ds::mesh mesh;
  float ocean_altitude;
  std::vector<glm::vec3> altitudes;
};

planet gen_planet() {
  auto sphere = ico_sphere_generator()();
  std::mt19937 mt(6342);
  std::uniform_real_distribution<float> urd(-1, 1);
  std::uniform_real_distribution<float> urd_dist(-1, 1);
  for (size_t i = 0; i < 1000; ++i) {
    glm::vec3 plane_normal = glm::normalize(glm::vec3({ urd(mt), urd(mt), urd(mt) }));
    float dist = urd_dist(mt);
    for (auto& vertex: sphere.vertices) {
      if (glm::dot(vertex.position, plane_normal) >= dist) {
        mod_altitude(vertex.position, 0.001f);
      } else {
        mod_altitude(vertex.position, -0.001f);
      }
    }
  }
  recenter_vertices(sphere.vertices);
  auto ocean_altitude = get_average_altitude(sphere.vertices);
  auto i = 0;
  std::vector<glm::vec3> altitudes(sphere.vertices.size());
  for (auto& vertex: sphere.vertices) {
    altitudes[i++] = vertex.position;
    auto length = glm::length(vertex.position);
    if (length < ocean_altitude) {
      vertex.position *= ocean_altitude / length;
    }
  }
  return {
    .mesh = sphere,
    .ocean_altitude = ocean_altitude,
    .altitudes = altitudes,
  };
}

int run(int argc, char* argv[]) {
  const auto options = parse_options(argc, argv);
  if (options.show_help) {
    return show_help();
  }
  auto context = create_context();
  glfwSetErrorCallback(errorCallback);
  auto window = create_window(context, options.window_mode);
  context.make_context_current(window);
  context.set_key_callback(window, keyCallback);
  enableGlew();

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
  glDepthFunc(GL_LESS);
  glFrontFace(GL_CCW);

  glpp::vertex_arrays<1> vao;
  glBindVertexArray(vao.handles()[0]);
  glpp::buffers<1> vbo;
  glpp::program program = ds::load_and_link_program(
    resources::shaders::BASIC_VS,
    resources::shaders::BASIC_FS
  );
  program.use();

  auto planet = gen_planet();
  auto object = planet.mesh;

  std::vector<GLfloat> colorData(object.vertices.size() * 3);
  for(int i = 0; i < colorData.size(); i += 3) {
    const auto& vertex = object.vertices[i / 3];
    const auto& alt = planet.altitudes[i / 3];
    auto length = glm::length(alt);
    if (length <= planet.ocean_altitude) {
      auto depth = glm::pow(length / planet.ocean_altitude, 5);
      colorData[i] = 0.1f * depth;
      colorData[i + 1] = 0.3f * depth;
      colorData[i + 2] = 0.6f * depth;
      continue;
    }
    auto height = (length - planet.ocean_altitude) / 0.3f;
    auto coef = height / 0.4f + 0.6f;
    colorData[i] = 0.9f * coef;
    colorData[i + 1] = 0.7f * coef;
    colorData[i + 2] = 0.7f * coef;
  }

  // Allocate space and upload the data from CPU to GPU
  auto objectVerticesByteCount = sizeof(ds::vertex) * object.vertices.size();
  auto colorsByteCount = sizeof(GLfloat) * colorData.size();
  glBindBuffer(GL_ARRAY_BUFFER, vbo.handles()[0]);
  auto totalSize = objectVerticesByteCount + colorsByteCount;
  glBufferData(GL_ARRAY_BUFFER, totalSize, nullptr, GL_STATIC_DRAW);
  glBufferSubData(GL_ARRAY_BUFFER, 0, objectVerticesByteCount, object.vertices.data());
  glBufferSubData(GL_ARRAY_BUFFER, objectVerticesByteCount, colorsByteCount, colorData.data());

  GLint positionAttribute = program.get_attrib_location("position");
  glVertexAttribPointer(
    positionAttribute,
    3,
    GL_FLOAT,
    GL_FALSE,
    sizeof(ds::vertex),
    reinterpret_cast<void*>(offsetof(ds::vertex, position))
  );
  glEnableVertexAttribArray(positionAttribute);

  GLint normalAttribute = program.get_attrib_location("normal");
  glVertexAttribPointer(
    normalAttribute,
    3,
    GL_FLOAT,
    GL_FALSE,
    sizeof(ds::vertex),
    reinterpret_cast<void*>(offsetof(ds::vertex, normal))
  );
  glEnableVertexAttribArray(normalAttribute);

  GLint colorAttribute = program.get_attrib_location("color");
  glVertexAttribPointer(colorAttribute, 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<void*>(objectVerticesByteCount));
  glEnableVertexAttribArray(colorAttribute);

  // Transfer the data from indices to eab
  GLuint eab;
  glGenBuffers(1, &eab);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eab);
  auto trianglesByteSize = sizeof(object.triangles[0]) * object.triangles.size();
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, trianglesByteSize, object.triangles.data(), GL_STATIC_DRAW);

  GLint modelUniform = program.get_uniform_location("Model");
  GLint viewUniform = program.get_uniform_location("View");
  GLint projectionUniform = program.get_uniform_location("Projection");

  glm::mat4 view = glm::lookAt(
    glm::vec3(2, 0, 0),
    glm::vec3(0, 0, 0),
    glm::vec3(0, 1, 0)
  );
  glUniformMatrix4fv(viewUniform, 1, GL_FALSE, glm::value_ptr(view));
  glm::mat4 projection = getPerspectiveProjection(window);
  glUniformMatrix4fv(projectionUniform, 1, GL_FALSE, glm::value_ptr(projection));

  auto rot = 0.0f;
  double targetDelta = 1.0 / 60.0;
  double lastTime = glfwGetTime();
  double extraTime = 0;

  while (!window.should_close()) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    auto ident = glm::mat4();
    auto model =
      glm::translate(ident, glm::vec3(0.0f, 0.0f, 0.0f)) *
      glm::rotate(ident, rot, glm::vec3(0.0f, 1.0f, 0.0f));
    glUniformMatrix4fv(modelUniform, 1, GL_FALSE, glm::value_ptr(model));
    rot += 0.005f;

    auto vertexCount = object.triangles.size() * 3;
    glDrawElements(GL_TRIANGLES, vertexCount, GL_UNSIGNED_INT, 0);

    window.swap_buffers();
    glfwPollEvents();

    double curTime = glfwGetTime();
    double elapsedDelta = curTime - lastTime;
    double spareDelta = extraTime + targetDelta - elapsedDelta;
    if (spareDelta > 0) {
      std::this_thread::sleep_for(std::chrono::microseconds(static_cast<int>(
        spareDelta * 1000 * 1000
      )));
    }

    curTime = glfwGetTime();
    elapsedDelta = curTime - lastTime;
    extraTime += targetDelta - elapsedDelta;
    lastTime = curTime;

  }
  return 0;
}

int main(int argc, char* argv[]) {
  try {
    run(argc, argv);
  } catch (ds::system_error error) {
    std::cout << "fatal: " << error.message << std::endl;
    return 2;
  }
}
