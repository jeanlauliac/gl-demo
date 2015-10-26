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

glpp::Shader getShaderFromFile(std::string filePath, GLenum shaderType) {
  std::ifstream ifs(filePath);
  std::string str(
    (std::istreambuf_iterator<char>(ifs)),
    std::istreambuf_iterator<char>()
  );

  glpp::Shader shader(shaderType);
  const char* cstr = str.c_str();
  shader.source(1, &cstr, nullptr);
  shader.compile();
  GLint status;
  shader.getShaderiv(GL_COMPILE_STATUS, &status);
  if (!status) {
    std::ostringstream errorMessage;
    errorMessage << "shader compilation failed:" << std::endl;
    GLint logLength;
    shader.getShaderiv(GL_INFO_LOG_LENGTH, &logLength);
    std::vector<char> log(logLength);
    shader.getInfoLog(log.size(), nullptr, &log[0]);
    errorMessage << &log[0] << std::endl;
    throw std::runtime_error(errorMessage.str());
  }
  return shader;
}

glpp::Program getProgramFromFiles(
  std::string vertexShaderPath,
  std::string fragmentShaderPath
) {
  glpp::Program program;
  program.attachShader(
    getShaderFromFile(vertexShaderPath, GL_VERTEX_SHADER)
  );
  program.attachShader(
    getShaderFromFile(fragmentShaderPath, GL_FRAGMENT_SHADER)
  );
  program.link();
  program.use();
  return program;
}

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

  // Create a Vector Buffer Object that will store the vertices on video memory
  GLuint vbo;
  glGenBuffers(1, &vbo);

  // Allocate space and upload the data from CPU to GPU
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(VERTEX_DATA), VERTEX_DATA, GL_STATIC_DRAW);

  while (!glfwWindowShouldClose(window.handle()))
  {
    float ratio;
    int width, height;
    glfwGetFramebufferSize(window.handle(), &width, &height);
    ratio = width / (float) height;
    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-ratio, ratio, -1.f, 1.f, 1.f, -1.f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glRotatef((float) glfwGetTime() * 50.f, 0.f, 0.f, 1.f);
    glBegin(GL_TRIANGLES);
    glColor3f(1.f, 0.f, 0.f);
    glVertex3f(-0.6f, -0.4f, 0.f);
    glColor3f(0.f, 1.f, 0.f);
    glVertex3f(0.6f, -0.4f, 0.f);
    glColor3f(0.f, 0.f, 1.f);
    glVertex3f(0.f, 0.6f, 0.f);
    glEnd();
    glfwSwapBuffers(window.handle());
    glfwPollEvents();
  }

}
