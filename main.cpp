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
#include "glpp/Program.h"
#include "glpp/Shader.h"
#include "ds/shaders.h"

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

GLfloat VERTEX_POSITIONS[] = {
  -0.5, -0.5, -0.5,
  0.5, -0.5, -0.5,
  0.5, 0.5, -0.5,
  -0.5, 0.5, -0.5,
  -0.5, -0.5, 0.5,
  0.5, -0.5, 0.5,
  0.5, 0.5, 0.5,
  -0.5, 0.5, 0.5,
};

GLuint INDICES[] = {
  0, 1, 2,
  2, 3, 0,
  4, 5, 6,
  6, 7, 4,
  0, 1, 4,
  5, 1, 4,
  3, 2, 7,
  7, 2, 6,
  1, 2, 5,
  5, 6, 2,
  0, 3, 7,
  0, 7, 4,
};

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

static void setupContext(glfwpp::Context& context) {
  context.windowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  context.windowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  context.windowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  context.windowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
  context.windowHint(GLFW_RESIZABLE, GL_FALSE);
}

int main(int argc, char* argv[]) {
  const auto options = parseOptions(argc, argv);
  if (options.showHelp) {
    return showHelp();
  }
  glfwpp::Context context;
  setupContext(context);
  glfwSetErrorCallback(errorCallback);
  glfwpp::Window window = createWindow(context, options.windowMode);
  context.makeContextCurrent(window);
  context.setKeyCallback(window, keyCallback);

  glewExperimental = GL_TRUE;
  GLenum err = glewInit();
  if(err != GLEW_OK) {
    throw std::runtime_error(
      std::string("cannot initialize GLEW: ") +
      reinterpret_cast<const char*>(glewGetErrorString(err))
    );
  }

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);

  GLuint vao;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);

  GLuint vbo;
  glGenBuffers(1, &vbo);

  GLuint eab;
  glGenBuffers(1, &eab);

  glpp::Program program = (
    ds::loadAndLinkProgram("shaders/basic.vs", "shaders/basic.fs")
  );
  program.use();

  GLfloat colorData[36];
  srand(42);
  int k = 0;
  for(int i = 0; i < sizeof(colorData) / sizeof(float) / 3; ++i) {
    float t = (float)rand()/(float)RAND_MAX;
    colorData[k] = 9*(1-t)*t*t*t;
    k++;
    colorData[k] = 15*(1-t)*(1-t)*t*t;
    k++;
    colorData[k] = 8.5*(1-t)*(1-t)*(1-t)*t;
    k++;
  }

  // Allocate space and upload the data from CPU to GPU
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(VERTEX_POSITIONS) + sizeof(colorData), nullptr, GL_STATIC_DRAW);
  glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(VERTEX_POSITIONS), VERTEX_POSITIONS);
  glBufferSubData(GL_ARRAY_BUFFER, sizeof(VERTEX_POSITIONS), sizeof(colorData), colorData);

  GLint positionAttribute = program.getAttribLocation("position");
  glVertexAttribPointer(positionAttribute, 3, GL_FLOAT, GL_FALSE, 0, 0);
  glEnableVertexAttribArray(positionAttribute);

  GLint colorAttribute = program.getAttribLocation("color");
  glVertexAttribPointer(colorAttribute, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid *)sizeof(VERTEX_POSITIONS));
  glEnableVertexAttribArray(colorAttribute);

  // Transfer the data from indices to eab
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eab);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(INDICES), INDICES, GL_STATIC_DRAW);

  GLint modelUniform = program.getUniformLocation("Model");
  GLint viewUniform = program.getUniformLocation("View");
  GLint projectionUniform = program.getUniformLocation("Projection");

  glm::mat4 view;
  glUniformMatrix4fv(viewUniform, 1, GL_FALSE, glm::value_ptr(view));

  int width, height;
  window.getFramebufferSize(&width, &height);
  float ratio = static_cast<float>(width) / static_cast<float>(height);
  glm::mat4 projection = glm::perspective(1.221f, ratio, 0.01f, 100.0f);

  auto rot = 0.0f;
  while (!window.shouldClose()) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUniformMatrix4fv(projectionUniform, 1, GL_FALSE, glm::value_ptr(projection));

    auto ident = glm::mat4();
    auto model =
      glm::translate(ident, glm::vec3(0.0f, 0.0f, -2.0f)) *
      glm::rotate(ident, 0.7f, glm::vec3(1.0f, 0.0f, 0.0f)) *
      glm::rotate(ident, rot, glm::vec3(0.0f, 1.0f, 0.0f));
      //glm::rotate(ident, rot2, glm::vec3(0.0f, 1.0f, 0.0f)) *
      //glm::rotate(ident, rot3, glm::vec3(0.0f, 0.0f, 1.0f));
    glUniformMatrix4fv(modelUniform, 1, GL_FALSE, glm::value_ptr(model));
    rot += 0.01f;

    glDrawElements(
      GL_TRIANGLES,
      sizeof(INDICES) / sizeof(GLuint),
      GL_UNSIGNED_INT,
      0
    );

    window.swapBuffers();
    glfwPollEvents();
  }

}
