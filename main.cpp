#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include "glfwpp/Context.h"
#include "glfwpp/Window.h"
#include "glpp/Program.h"
#include "glpp/Shader.h"
#include "demoscene/shaders.h"

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

GLfloat VERTEX_DATA[24] = {
  0.0, 0.0,
  0.5, 0.0,
  0.5, 0.5,

  0.0, 0.0,
  0.0, 0.5,
  -0.5, 0.5,

  0.0, 0.0,
  -0.5, 0.0,
  -0.5, -0.5,

  0.0, 0.0,
  0.0, -0.5,
  0.5, -0.5,
};

int main() {
  glfwSetErrorCallback(errorCallback);

  glfwpp::Context context;
  context.windowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  context.windowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  context.windowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  context.windowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

  glfwpp::Window window;

  glewExperimental = GL_TRUE;
  if(glewInit() != GLEW_OK) {
    throw std::runtime_error("cannot initialize GLEW");
  }

  context.makeContextCurrent(window);
  context.setKeyCallback(window, keyCallback);

  GLuint vao;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);

  GLuint vbo;
  glGenBuffers(1, &vbo);

  glpp::Program program = (
    demoscene::loadAndLinkProgram("shaders/basic.vs", "shaders/basic.fs")
  );
  program.use();

  // Allocate space and upload the data from CPU to GPU
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(VERTEX_DATA), VERTEX_DATA, GL_STATIC_DRAW);

  GLint positionAttribute = program.getAttribLocation("position");
  glVertexAttribPointer(positionAttribute, 2, GL_FLOAT, GL_FALSE, 0, 0);
  glEnableVertexAttribArray(positionAttribute);

  while (!window.shouldClose()) {
    glClear(GL_COLOR_BUFFER_BIT);

    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 12);

    window.swapBuffers();
    glfwPollEvents();
  }

}
