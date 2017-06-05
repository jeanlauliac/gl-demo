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
  vec3 lightDir = normalize(vec3(0.1, 0.3, 1.0));
  float power = clamp(dot(worldNormal, vec4(lightDir, 1)), 0, 1);
  edge_color = color * power;
}
