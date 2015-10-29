#version 150

uniform mat4 Model;
uniform mat4 View;
uniform mat4 Projection;

in vec4 position;
in vec4 color;
out vec4 edge_color;

void main() {
  gl_Position = Projection * View * Model * position;
  edge_color = color;
}
