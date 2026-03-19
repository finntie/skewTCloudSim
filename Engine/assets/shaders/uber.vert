#version 460 core
#extension GL_GOOGLE_include_directive : require

#include "locations.glsl"
#include "uniforms.glsl"

layout (location = POSITION_LOCATION) in vec3 a_position;
layout (location = NORMAL_LOCATION) in vec3 a_normal;
layout (location = TANGENT_LOCATION) in vec4 a_tangent;
layout (location = TEXTURE0_LOCATION) in vec2 a_texture0;
layout (location = TEXTURE1_LOCATION) in vec2 a_texture1;

out vec3 v_position;
out vec3 v_normal;
out vec3 v_tangent;
out vec2 v_texture0;
out vec2 v_texture1;

void main()
{       
    mat4 world = bee_transforms[gl_InstanceID].world;
    mat4 wv = bee_view * world;
    v_position = (world * vec4(a_position, 1.0)).xyz;
    v_normal = normalize((world * vec4(a_normal, 0.0)).xyz);
    v_tangent = normalize((world * vec4(a_tangent.xyz, 0.0)).xyz);
    v_texture0 = a_texture0;
    v_texture1 = a_texture1;
    mat4 wvp = bee_transforms[gl_InstanceID].wvp;
    gl_Position = wvp * vec4(a_position, 1.0);
}