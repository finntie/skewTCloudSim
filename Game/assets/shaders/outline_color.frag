#version 460 core
#extension GL_GOOGLE_include_directive : require

out vec4 FragColor;

uniform vec4 base_color_factor;

void main()
{
    FragColor = vec4(1.0, 1.0, 1.0, 1.0) * base_color_factor;
}