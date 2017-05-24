#include "ds/cube.h"
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
#include <sstream>
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
  glDepthFunc(GL_LESS);

  glpp::vertex_arrays<1> vao;
  glBindVertexArray(vao.handles()[0]);
  glpp::buffers<1> vbo;
  glpp::program program = ds::load_and_link_program(
    resources::shaders::BASIC_VS,
    resources::shaders::BASIC_FS
  );
  program.use();

  auto cube = ds::get_cube();

  std::vector<GLfloat> colorData(cube.vertices.size() * 3);
  srand(42);
  for(int i = 0; i < colorData.size(); i += 3) {
    float t = (float)rand()/(float)RAND_MAX;
    colorData[i] = 9*(1-t)*t*t*t;
    colorData[i + 1] = 15*(1-t)*(1-t)*t*t;
    colorData[i + 2] = 8.5*(1-t)*(1-t)*(1-t)*t;
  }

  // Allocate space and upload the data from CPU to GPU
  auto cubeVerticesByteCount = sizeof(ds::vertex) * cube.vertices.size();
  auto colorsByteCount = sizeof(GLfloat) * colorData.size();
  glBindBuffer(GL_ARRAY_BUFFER, vbo.handles()[0]);
  auto totalSize = cubeVerticesByteCount + colorsByteCount;
  glBufferData(GL_ARRAY_BUFFER, totalSize, nullptr, GL_STATIC_DRAW);
  glBufferSubData(GL_ARRAY_BUFFER, 0, cubeVerticesByteCount, cube.vertices.data());
  glBufferSubData(GL_ARRAY_BUFFER, cubeVerticesByteCount, colorsByteCount, colorData.data());

  GLint positionAttribute = program.getAttribLocation("position");
  glVertexAttribPointer(
    positionAttribute,
    3,
    GL_FLOAT,
    GL_FALSE,
    sizeof(ds::vertex),
    reinterpret_cast<void*>(offsetof(ds::vertex, position))
  );
  glEnableVertexAttribArray(positionAttribute);

  GLint normalAttribute = program.getAttribLocation("normal");
  glVertexAttribPointer(
    normalAttribute,
    3,
    GL_FLOAT,
    GL_FALSE,
    sizeof(ds::vertex),
    reinterpret_cast<void*>(offsetof(ds::vertex, normal))
  );
  glEnableVertexAttribArray(normalAttribute);

  GLint colorAttribute = program.getAttribLocation("color");
  glVertexAttribPointer(colorAttribute, 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<void*>(cubeVerticesByteCount));
  glEnableVertexAttribArray(colorAttribute);

  // Transfer the data from indices to eab
  GLuint eab;
  glGenBuffers(1, &eab);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eab);
  auto trianglesByteSize = sizeof(cube.triangles[0]) * cube.triangles.size();
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, trianglesByteSize, cube.triangles.data(), GL_STATIC_DRAW);

  GLint modelUniform = program.getUniformLocation("Model");
  GLint viewUniform = program.getUniformLocation("View");
  GLint projectionUniform = program.getUniformLocation("Projection");

  glm::mat4 view;
  glUniformMatrix4fv(viewUniform, 1, GL_FALSE, glm::value_ptr(view));
  glm::mat4 projection = getPerspectiveProjection(window);
  glUniformMatrix4fv(projectionUniform, 1, GL_FALSE, glm::value_ptr(projection));

  auto rot = 0.0f;
  while (!window.should_close()) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    auto ident = glm::mat4();
    auto model =
      glm::translate(ident, glm::vec3(0.0f, 0.0f, -2.0f)) *
      glm::rotate(ident, 0.7f, glm::vec3(1.0f, 0.0f, 0.0f)) *
      glm::rotate(ident, rot, glm::vec3(0.0f, 1.0f, 0.0f));
    glUniformMatrix4fv(modelUniform, 1, GL_FALSE, glm::value_ptr(model));
    rot += 0.01f;

    auto vertexCount = cube.triangles.size() * 3;
    glDrawElements(GL_TRIANGLES, vertexCount, GL_UNSIGNED_INT, 0);

    window.swap_buffers();
    glfwPollEvents();
  }
  return 0;
}

int main(int argc, char* argv[]) {
  try {
    run(argc, argv);
  } catch (ds::system_error error) {
    std::cout << "Oooops: " << error.message << std::endl;
    return 2;
  }
}
