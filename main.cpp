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

GLfloat VERTEX_DATA[] = {
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
  glBufferData(GL_ARRAY_BUFFER, sizeof(VERTEX_DATA) + sizeof(colorData), nullptr, GL_STATIC_DRAW);
  glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(VERTEX_DATA), VERTEX_DATA);
  glBufferSubData(GL_ARRAY_BUFFER, sizeof(VERTEX_DATA), sizeof(colorData), colorData);

  GLint positionAttribute = program.getAttribLocation("position");
  glVertexAttribPointer(positionAttribute, 2, GL_FLOAT, GL_FALSE, 0, 0);
  glEnableVertexAttribArray(positionAttribute);

  GLint colorAttribute = program.getAttribLocation("color");
  glVertexAttribPointer(colorAttribute, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid *)sizeof(VERTEX_DATA));
  glEnableVertexAttribArray(colorAttribute);

  while (!window.shouldClose()) {
    glClear(GL_COLOR_BUFFER_BIT);

    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 12);

    window.swapBuffers();
    glfwPollEvents();
  }

}
