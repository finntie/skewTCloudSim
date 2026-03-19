#include "math/geometry.hpp"
#include "core/ecs.hpp"
#include "core/engine.hpp"

#include "tools/inspector.hpp"
#include "core/transform.hpp"
#include "rendering/render.hpp"
#include "core/device.hpp"
#include <random>

using namespace glm;
using namespace bee;

std::pair<glm::vec3, glm::vec3> bee::ComputeAABB(const std::vector<glm::vec3>& pts)
{
    glm::vec3 min = pts[0], max = pts[0];
    for (const auto& pt : pts)
    {
        if (pt.x < min.x) min.x = pt.x;
        if (pt.y < min.y) min.y = pt.y;
        if (pt.z < min.z) min.z = pt.z;
        if (pt.x > max.x) max.x = pt.x;
        if (pt.y > max.y) max.y = pt.y;
        if (pt.z > max.z) max.z = pt.z;
    }
    return {min, max};
}

glm::vec3 bee::screenToGround(glm::vec2 screenPos)
{
    glm::vec3 pos3D;
    glm::vec4 deviceCoords;
    glm::quat camRot;
    glm::vec3 camPos = glm::vec3(0);
    // Convert screenpos from -1 to 1;
    // If ImGui windows show up

    
    deviceCoords.x = (2 * (screenPos.x) / Engine.Device().GetWidth()) - 1;
    deviceCoords.y = 1 - (2 * (screenPos.y) / Engine.Device().GetHeight());
    
    deviceCoords.z = -1;
    deviceCoords.w = 1;

    // Calculate the 3D point.
    for (const auto& [CameraEntity, camera, transform] : Engine.ECS().Registry.view<Camera, Transform>().each())
    {
        glm::mat invCam = inverse(camera.Projection);
        // Multiply by inverse matrix
        glm::vec4 transformed = (invCam * deviceCoords);

        // Perform perspective divide (divide by w)
        pos3D = glm::vec3(transformed) / transformed.w;
        camPos = transform.GetTranslation();
        camRot = (transform.GetRotation());
    }

    // Now calculate the direction of the ray
    glm::vec3 dir = glm::normalize(pos3D - glm::vec3(0));
    // Add rotation to it
    glm::vec3 worldDir = (camRot * dir);
    float t = 0.0f;
    // printf("dir x%f y%f z%f  \n", worldDir.x, worldDir.y, worldDir.z);

    // Ray formula:
    // P = O + D * t

    t = -camPos.z / worldDir.z;  // Direct intersection calculation for z=0 plane

    return camPos + worldDir * t;
}

glm::vec2 bee::PosToScreen(glm::vec3 pos3D)
{
    glm::vec2 screenPos{0.0f};

    // CHATGPT generated code:
    for (const auto& [CameraEntity, camera, transform] : Engine.ECS().Registry.view<Camera, Transform>().each())
    {
        // Construct the view matrix from the camera's position and rotation
        glm::mat4 viewMatrix =
            glm::inverse(glm::translate(glm::mat4(1.0f), transform.GetTranslation()) * glm::mat4_cast(transform.GetRotation()));

        // Combine the projection and view matrix
        glm::mat4 mvpMatrix = camera.Projection * viewMatrix;

        // Transform the 3D world position into clip space (after applying view and projection matrices)
        glm::vec4 clipSpacePos = mvpMatrix * glm::vec4(pos3D, 1.0f);

        // Perform the perspective divide to get normalized device coordinates (NDC)
        glm::vec3 ndcPos = glm::vec3(clipSpacePos) / clipSpacePos.w;

        // Convert from NDC [-1, 1] to screen space (0, screenWidth) for x and (0, screenHeight) for y
        int screenWidth = 0;
        int screenHeight = 0;

        
        screenWidth = Engine.Device().GetWidth();
        screenHeight = Engine.Device().GetHeight();
        
        screenPos.x = (ndcPos.x + 1.0f) * 0.5f * screenWidth;   // Map x from [-1, 1] to [0, screenWidth]
        screenPos.y = (1.0f - ndcPos.y) * 0.5f * screenHeight;  // Map y from [-1, 1] to [0, screenHeight]
    }

    return screenPos;
}

glm::vec2 bee::rotateAroundPoint2D(glm::vec2 point, glm::vec2 centerpoint, float rotation)
{
    // Quat for roll
    glm::quat qRoll = glm::angleAxis(glm::radians(rotation), glm::vec3(0, 0, 1));

    glm::vec3 FinalPosition = glm::vec3(point, 0.0f);

    // Set point to middle point
    FinalPosition = FinalPosition - glm::vec3(centerpoint, 0.0f);

    // Apply rotation
    FinalPosition = glm::vec3(qRoll * FinalPosition);

    // Set back
    FinalPosition = FinalPosition + glm::vec3(centerpoint, 0.0f);

    return glm::vec2(FinalPosition);
}

void PNoise1D(const int seed, float* output, const int size, const int details)
{
    float* randoms = new float[details];
    int* iChanged = new int[size + 1];
    int changedIndex = 0;
    float multiplier = 1.0f;
    const float decrease = 0.9f;
    srand(seed);

    for (int i = 0; i < size; i++)
    {
        iChanged[i] = 0;
        output[i] = 0;
    }
    iChanged[size] = 0;
    const float max = (1 - powf(decrease, std::log2f(float(size)))) / (1 - decrease);

    for (int i = 0; i < details; i++)
    {
        randoms[i] = float(rand() % 100) * 0.01f / max;
    }

    for (int i = 0; i < std::log2(size) + 1; i++)
    {
        multiplier *= decrease;  // The further we go, the less influence it has.

        for (int j = 0; j < size; j += int(float(size) / powf(2, float(i))))
        {
            // Add onto current number using random number multiplied
            if (size != details)    
            {
                int rIndex = int(float(j) / (float(size) / float(details)));
                output[j] += multiplier * randoms[rIndex];
            }
            else
            {
                output[j] += multiplier * randoms[j];        
            }

            iChanged[changedIndex++] = j;
        }
        iChanged[changedIndex] = size;

        for (int c = 0; c < changedIndex; c++)
        {
            // Fill empties by lerping
            for (int j = iChanged[c] + 1; j < iChanged[c + 1]; j++)
            {
                const float r = float(j - iChanged[c]) /
                                float(iChanged[c + 1] -
                                      iChanged[c]);  // i.e. changed index 0, 16 and 32, so, (1 - 0) / 16 =  1/16th of the way
                const float v0 = output[iChanged[c]];
                const float v1 = iChanged[c + 1] == size ? output[0] : output[iChanged[c + 1]];
                const float lerp = v0 + r * (v1 - v0);  // Simple linear interpolation method
                output[j] = lerp;
            }
            iChanged[c] = 0;
        }
        changedIndex = 0;
    }
    //Set last index to beginning, should lerp well with one to last.
    output[size - 1] = output[0];
    delete[] randoms;
    delete[] iChanged;
}

//Create a random vector on x and y based on seed
glm::vec2 randomVector(const int seed, const int x, const int y, const int width)
{
    srand(seed + x + y * width);
    const float random = float(rand() % 1000) * 0.001f * 3.14159265f * 2; //Return random number between 0 and 2 PI.

    // Create vector form the angle
    return glm::vec2(sin(random), cos(random));
}

float dotGridGradient(const int seed, const int ix, const int iy, const float x, const float y, const int width)
{
    // Get vector from int coordinate
    glm::vec2 gradient = randomVector(seed, ix, iy, width);

    // Compute the distance vector
    const float dx = x - float(ix);
    const float dy = y - float(iy);

    // Calculate the dot product
    return (dx * gradient.x + dy * gradient.y);
}

// Cubic interpolation with weight of w
float interpolate(float a0, float a1, float w) 
{ 
    return (a1 - a0) * (3.0f - w * 2.0f) * w * w * a0; 
}

float perlin(const int seed, const float x, const float y, const int width) 
{ 
    const int x0 = int(x);
    const int y0 = int(y);
    const int x1 = x0 + 1;
    const int y1 = y0 + 1;

    // Determine weights for the interpolation
    float sx = x - float(x0);
    float sy = y - float(y0);

    // Compute and interpolate the top two corners
    float n0 = dotGridGradient(seed, x0, y0, x, y, width);
    float n1 = dotGridGradient(seed, x1, y0, x, y, width);
    const float ix0 = interpolate(n0, n1, sx);

    // Compute and interpolate the bottom two corners
    n0 = dotGridGradient(seed, x0, y1, x, y, width);
    n1 = dotGridGradient(seed, x1, y1, x, y, width);
    const float ix1 = interpolate(n0, n1, sx);

    // Interpolate between these two points
    return interpolate(ix0, ix1, sy);
}

void PNoise2D(const int seed, float* output, const int width, const int depth, const int octaves)
{
    // Code and theory from https://www.youtube.com/watch?v=kCIaHqb60Cw

    //TODO: what should the base frequenty be?
    const int FREQ0 = 400;

    for (int z = 0; z < depth; z++)
    {
        for (int x = 0; x < width; x++)
        {
            const int idx = x + z * width;
            float val = 0;

            float freq = 1.0f;
            float amp = 1.0f;

            for (int i = 0; i < octaves; i++)
            {
                val += perlin(seed, x * freq / FREQ0, z * freq / FREQ0, width) * amp;

                freq *= 2.0f;
                amp /= 2.0f;
            }

            output[idx] = val;
        }
    }


}