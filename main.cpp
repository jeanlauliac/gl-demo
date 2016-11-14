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
  -0.5, -0.5,
  0.5, -0.5,
  0.5, 0.5,
  -0.5, 0.5,
};

GLuint INDICES[6] = {
  0, 1, 2,
  2, 3, 0
};

enum class WindowMode { WINDOW, FULLSCREEN, };

struct Options {
  Options(): showHelp(false), windowMode(WindowMode::WINDOW) {}

  bool showHelp;
  WindowMode windowMode;
};

Options parseOptions(int argc, char* argv[]) {
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

int showHelp() {
  std::cout << R"END(Usage: gl-demo [options]
Options:
  --fullscreen, -f          Create a fullscreen window
  --help, -h                Show this
)END";
  return 0;
}

glfwpp::Window createWindow(glfwpp::Context& context, WindowMode windowMode) {
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

int main(int argc, char* argv[]) {
  const auto options = parseOptions(argc, argv);
  if (options.showHelp) {
    return showHelp();
  }

  glfwSetErrorCallback(errorCallback);

  glfwpp::Context context;

  context.windowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  context.windowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  context.windowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  context.windowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
  context.windowHint(GLFW_RESIZABLE, GL_FALSE);

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
  glVertexAttribPointer(positionAttribute, 2, GL_FLOAT, GL_FALSE, 0, 0);
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

  auto rot = 0.0f;
  auto rot2 = 0.5f;
  while (!window.shouldClose()) {
    glClear(GL_COLOR_BUFFER_BIT);

    int width, height;
    window.getFramebufferSize(&width, &height);
    float ratio = static_cast<float>(width) / static_cast<float>(height);
    //glm::mat4 projection = glm::ortho(-ratio, ratio, -1.0f, 1.0f, -1.0f, 1.0f);
    glm::mat4 projection = glm::perspective(3, ratio, 10, 1000);
    glUniformMatrix4fv(projectionUniform, 1, GL_FALSE, glm::value_ptr(projection));

    glm::mat4 model = glm::rotate(glm::mat4(), rot, glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::rotate(model, rot2, glm::vec3(0.0f, 1.0f, 0.0f));
    glUniformMatrix4fv(modelUniform, 1, GL_FALSE, glm::value_ptr(model));
    rot += 0.01f;
    rot2 += 0.008f;

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    window.swapBuffers();
    glfwPollEvents();
  }

}
