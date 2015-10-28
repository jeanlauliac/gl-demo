#version 150

in vec4 position;
in vec4 color;
out vec4 edge_color;

void main() {
  gl_Position = position;
  edge_color = color;
}
