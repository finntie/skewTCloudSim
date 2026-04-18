#version 460 core
#extension GL_GOOGLE_include_directive : require

#include "locations.glsl"
#include "uniforms.glsl"

layout (location = POSITION_LOCATION) in vec3 a_position;

void main()
{
    mat4 wvp = bee_transforms[gl_InstanceID].wvp;
    gl_Position = wvp * vec4(a_position, 1.0);
}