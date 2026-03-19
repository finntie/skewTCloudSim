#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>

namespace bee
{
std::pair<glm::vec3, glm::vec3> ComputeAABB(const std::vector<glm::vec3>& pts);
glm::vec3 screenToGround(glm::vec2 screenPos);
glm::vec2 PosToScreen(glm::vec3 pos3D);
glm::vec2 rotateAroundPoint2D(glm::vec2 point, glm::vec2 centerpoint, float rotation);
}  // namespace bee

/// <summary>Creates smoothed 1D pearling noise</summary>
/// <param name="seed">Seed for the randomizer</param>
/// <param name="output">Output array where the noise will be stored (values between 0 and 1)</param>
/// <param name="size">Size of the output array</param>
/// <param name="details">How many different random numbers go into the formula, normal value would be the same or less as 'size'</param>
void PNoise1D(const int seed, float* output, const int size, const int details = 32);

/// <summary>Creates smoothed 2D pearling noise</summary>
/// <param name="seed">Seed for the randomizer</param>
/// <param name="output">Output array where the noise will be stored (values between 0 and 1)</param>
/// <param name="width">width of the output array</param>
/// <param name="depth">Depth of the output array</param>
/// <param name="octaves">How many detail layers do we want? more octaves could cause perlin to be slower</param>
void PNoise2D(const int seed, float* output, const int width, const int depth, const int octaves = 12);