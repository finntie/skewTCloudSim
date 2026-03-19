#pragma once
#include <glm/glm.hpp>
#define vec2 glm::vec2
#define vec3 glm::vec3
#define vec4 glm::vec4
#define mat4 glm::mat4
#define mat3 glm::mat3
#define uniform struct
#define layout(x, y)

#include "../../../assets/shaders/locations.glsl"

namespace bee
{
#include "../../../assets/shaders/uniforms.glsl"
}
