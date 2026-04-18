#version 460 core
#extension GL_ARB_explicit_uniform_location : enable
layout (location = 0) in vec3 a_position;
layout (location = 1) in vec2 a_texture;

out vec2 tex_coords;

void main()
{
  gl_Position = vec4(a_position, 1.0);
  tex_coords = a_texture;
}