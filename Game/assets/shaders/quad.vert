#version 460 core
#extension GL_ARB_explicit_uniform_location : enable

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec2 a_texture;
out vec2 v_texture;

void main(void)
{
    v_texture = a_texture;
    gl_Position = vec4(a_position, 1.0);  
}
