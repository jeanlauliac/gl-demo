#version 150

uniform mat4 Model;
uniform mat4 View;
uniform mat4 Projection;

in vec4 position;
in vec4 normal;
in vec4 color;
out vec4 edge_color;

void main() {
  gl_Position = Projection * View * Model * position;
  vec4 worldNormal = Model * normal;
  float power = clamp(dot(worldNormal, vec4(0.0, 1.0, 0.0, 1.0)), 0, 1);
  edge_color = color * power;
}
