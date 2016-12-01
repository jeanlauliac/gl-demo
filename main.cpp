#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include "glfwpp/Context.h"
#include "glfwpp/Window.h"
#include "glpp/Buffers.h"
#include "glpp/Program.h"
#include "glpp/Shader.h"
#include "glpp/VertexArrays.h"
#include "ds/shaders.h"
#include "ds/SystemException.h"
#include "resources/index.h"

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

enum class WindowMode { WINDOW, FULLSCREEN, };

struct Options {
  Options(): showHelp(false), windowMode(WindowMode::WINDOW) {}

  bool showHelp;
  WindowMode windowMode;
};

static Options parseOptions(int argc, char* argv[]) {
  Options options;
  for (++argv, --argc; argc > 0; ++argv, --argc) {
    const auto arg = std::string(*argv);
    if (arg == "--fullscreen" || arg == "-f") {
      options.windowMode = WindowMode::FULLSCREEN;
    } else if (arg == "--help" || arg == "-h") {
      options.showHelp = true;
    } else {
      throw std::runtime_error("unknown argument: `" + arg + "`");
    }
  }
  return options;
}

static int showHelp() {
  std::cout << R"END(Usage: gl-demo [options]
Options:
  --fullscreen, -f          Create a fullscreen window
  --help, -h                Show this
)END";
  return 0;
}

static glfwpp::Window createWindow(
  glfwpp::Context& context,
  WindowMode windowMode
) {
  if (windowMode == WindowMode::WINDOW) {
    return glfwpp::Window(800, 600, "Demo", nullptr, nullptr);
  }
  GLFWmonitor* monitor = glfwGetPrimaryMonitor();
  const GLFWvidmode* mode = glfwGetVideoMode(monitor);
  context.windowHint(GLFW_RED_BITS, mode->redBits);
  context.windowHint(GLFW_GREEN_BITS, mode->greenBits);
  context.windowHint(GLFW_BLUE_BITS, mode->blueBits);
  context.windowHint(GLFW_REFRESH_RATE, mode->refreshRate);
  return glfwpp::Window(mode->width, mode->height, "Demo", monitor, nullptr);
}

static glfwpp::Context createContext() {
  glfwpp::Context context;
  context.windowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  context.windowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  context.windowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  context.windowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
  context.windowHint(GLFW_RESIZABLE, GL_FALSE);
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

glm::mat4 getPerspectiveProjection(const glfwpp::Window& window) {
  int width, height;
  window.getFramebufferSize(&width, &height);
  float ratio = static_cast<float>(width) / static_cast<float>(height);
  return glm::perspective(1.221f, ratio, 0.01f, 100.0f);
}

glm::vec3 CUBE_FACES[] = {
  glm::vec3(-0.5, 0, 0),
  glm::vec3(0.5, 0, 0),
  glm::vec3(0, -0.5, 0),
  glm::vec3(0, 0.5, 0),
  glm::vec3(0, 0, -0.5),
  glm::vec3(0, 0, 0.5),
};

struct Vertex {
  glm::vec3 position;
  glm::vec3 normal;
};

struct Mesh {
  std::vector<Vertex> vertices;
  std::vector<glm::uvec3> triangles;
};

Mesh createCube() {
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

std::ostream &operator<<(std::ostream &os, const glm::vec3& vec) {
  return os << "[" << vec.x << ", " << vec.y << ", " << vec.z << "]";
}

std::ostream &operator<<(std::ostream &os, const Vertex& vertex) {
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
  const auto options = parseOptions(argc, argv);
  if (options.showHelp) {
    return showHelp();
  }
  auto context = createContext();
  glfwSetErrorCallback(errorCallback);
  auto window = createWindow(context, options.windowMode);
  context.makeContextCurrent(window);
  context.setKeyCallback(window, keyCallback);
  enableGlew();

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);

  glpp::VertexArrays<1> vao;
  glBindVertexArray(vao.handles()[0]);
  glpp::Buffers<1> vbo;
  glpp::Program program =
    ds::loadAndLinkProgram("shaders/basic.vs", "shaders/basic.fs");
  program.use();

  auto cube = createCube();

  std::vector<GLfloat> colorData(cube.vertices.size() * 3);
  srand(42);
  for(int i = 0; i < colorData.size(); i += 3) {
    float t = (float)rand()/(float)RAND_MAX;
    colorData[i] = 9*(1-t)*t*t*t;
    colorData[i + 1] = 15*(1-t)*(1-t)*t*t;
    colorData[i + 2] = 8.5*(1-t)*(1-t)*(1-t)*t;
  }

  // Allocate space and upload the data from CPU to GPU
  auto cubeVerticesByteCount = sizeof(Vertex) * cube.vertices.size();
  auto colorsByteCount = sizeof(GLfloat) * colorData.size();
  glBindBuffer(GL_ARRAY_BUFFER, vbo.handles()[0]);
  auto totalSize = cubeVerticesByteCount + colorsByteCount;
  glBufferData(GL_ARRAY_BUFFER, totalSize, nullptr, GL_STATIC_DRAW);
  glBufferSubData(GL_ARRAY_BUFFER, 0, cubeVerticesByteCount, cube.vertices.data());
  glBufferSubData(GL_ARRAY_BUFFER, cubeVerticesByteCount, colorsByteCount, colorData.data());

  GLint positionAttribute = program.getAttribLocation("position");
  glVertexAttribPointer(positionAttribute, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, position)));
  glEnableVertexAttribArray(positionAttribute);

  GLint normalAttribute = program.getAttribLocation("normal");
  glVertexAttribPointer(normalAttribute, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));
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
  while (!window.shouldClose()) {
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

    window.swapBuffers();
    glfwPollEvents();
  }
  return 0;
}

int main(int argc, char* argv[]) {
  try {
    run(argc, argv);
  } catch (ds::SystemException ex) {
    std::cout << "Oooops: " << ex.message << std::endl;
    return 2;
  }
}
