#include "rendering/debug_render.hpp"

#include <glm/gtc/constants.hpp>
#include <glm/gtx/quaternion.hpp>

using namespace bee;
using namespace glm;

void DebugRenderer::AddCircle(DebugCategory::Enum category,
                              const vec3& center,
                              float radius,
                              const glm::vec3& normal,
                              const vec4& color)
{
    if (!(m_categoryFlags & category)) return;

    constexpr float dt = glm::two_pi<float>() / 32.0f;
    float t = 0.0f;

    const auto& rotation = glm::rotation(vec3(0, 0, 1), normal);

    vec3 v0 = center + radius * glm::rotate(rotation, vec3(cos(t), sin(t), 0));
    for (; t < glm::two_pi<float>(); t += dt)
    {
        vec3 v1 = center + radius * glm::rotate(rotation, vec3(cos(t + dt), sin(t + dt), 0));
        AddLine(category, v0, v1, color);
        v0 = v1;
    }
}

void DebugRenderer::AddSquare(DebugCategory::Enum category,
                              const glm::vec3& center,
                              float size,
                              const glm::vec3& normal,
                              const glm::vec4& color)
{
    if (!(m_categoryFlags & category)) return;

    const auto& rotation = glm::rotation(vec3(0, 0, 1), normal);

    const float s = size * 0.5f;
    auto A = center + glm::rotate(rotation, vec3(-s, -s, 0.0f));
    auto B = center + glm::rotate(rotation, vec3(-s, s, 0.0f));
    auto C = center + glm::rotate(rotation, vec3(s, s, 0.0f));
    auto D = center + glm::rotate(rotation, vec3(s, -s, 0.0f));

    // TODO: use normal

    AddLine(category, A, B, color);
    AddLine(category, B, C, color);
    AddLine(category, C, D, color);
    AddLine(category, D, A, color);
}

void bee::DebugRenderer::AddFilledSquare(DebugCategory::Enum category,
                                         const glm::vec3& center,
                                         float size,
                                         const glm::vec3& normal,
                                         const glm::vec4& color)
{
    if (!(m_categoryFlags & category)) return;

    const auto& rotation = glm::rotation(vec3(0, 0, 1), normal);

    const float s = size * 0.5f;
    auto A = center + glm::rotate(rotation, vec3(-s, -s, 0.0f));
    auto B = center + glm::rotate(rotation, vec3(-s, s, 0.0f));
    auto C = center + glm::rotate(rotation, vec3(s, s, 0.0f));
    auto D = center + glm::rotate(rotation, vec3(s, -s, 0.0f));

    // TODO: use normal

    AddTriangle(category, A, B, C, color);
    AddTriangle(category, C, D, A, color);
}

void bee::DebugRenderer::AddRectangle(DebugCategory::Enum category,
                                      const glm::vec3& from,
                                      const glm::vec3& to,
                                      const glm::vec3& normal,
                                      const glm::vec4& color)
{
    if (!(m_categoryFlags & category)) return;

    const glm::vec3 diagonal = to - from;
    const glm::vec3 middle = from + diagonal * 0.5f;
    const glm::vec3 side = glm::cross(normal, diagonal) * 0.5f;

    const glm::vec3 A = from;
    const glm::vec3 B = middle + side;
    const glm::vec3 C = to;
    const glm::vec3 D = middle - side;

    AddLine(category, A, B, color);
    AddLine(category, B, C, color);
    AddLine(category, C, D, color);
    AddLine(category, D, A, color);
}

void DebugRenderer::AddCylinder(DebugCategory::Enum category,
                                const glm::vec3& center1,
                                const glm::vec3& center2,
                                float radius,
                                const vec4& color)
{
    if (!(m_categoryFlags & category)) return;

    constexpr float dt = glm::two_pi<float>() / 16.0f;
    float t = 0.0f;

    const auto& diff = center2 - center1;
    const auto& rotation = glm::rotation(vec3(0, 0, 1), glm::normalize(diff));

    vec3 v0 = center1 + radius * glm::rotate(rotation, vec3(cos(t), sin(t), 0));
    for (; t < glm::two_pi<float>(); t += dt)
    {
        vec3 v1 = center1 + radius * glm::rotate(rotation, vec3(cos(t + dt), sin(t + dt), 0));
        AddLine(category, v0, v1, color);
        AddLine(category, v0 + diff, v1 + diff, color);
        AddLine(category, v0, v0 + diff, color);
        v0 = v1;
    }
}

void bee::DebugRenderer::AddArrow(DebugCategory::Enum category,
                                  const glm::vec3& center,
                                  const glm::vec3& normal,
                                  const glm::vec3& pointDir,
                                  float size,
                                  const glm::vec4& color)
{
    if (!(m_categoryFlags & category)) return;

    const glm::vec3 forward = glm::normalize(pointDir);
    const glm::vec3 side = glm::normalize(glm::cross(normal, forward));

    const float s = size * 0.5f;

    auto A = center - forward * s;  // tail
    auto B = center + forward * s;  // tip

    auto C = B - forward * (s * 0.5f) + side * (s * 0.5f);
    auto D = B - forward * (s * 0.5f) - side * (s * 0.5f);

    AddLine(category, A, B, color);
    AddLine(category, B, C, color);
    AddLine(category, B, D, color);
}

void bee::DebugRenderer::AddVoxel(DebugCategory::Enum category, const glm::vec3& center, float size, const glm::vec4& color) 
{
    // Add rectangles on all 6 sides

    // First add left and right plane (x values)

    float halfSize = size * 0.5f;

    glm::vec3 normal = glm::normalize(center - glm::vec3(center.x + halfSize, center.y, center.z));
    glm::vec3 fromPos = center + glm::vec3(halfSize, -halfSize, -halfSize); // Move half to right, back and down
    glm::vec3 toPos = fromPos + glm::vec3(0, size, size); // Move fully back
    AddRectangle(category, fromPos, toPos, normal, color);

    // Left
    normal = glm::normalize(center - glm::vec3(center.x - halfSize, center.y, center.z));
    fromPos.x -= size;
    toPos.x -= size;
    AddRectangle(category, fromPos, toPos, normal, color);

    AddLine(category, fromPos, glm::vec3(fromPos.x + size, fromPos.y, fromPos.z), color);
    AddLine(category,
            glm::vec3(fromPos.x, fromPos.y + size, fromPos.z),
            glm::vec3(fromPos.x + size, fromPos.y + size, fromPos.z),
            color);
    AddLine(category, toPos, glm::vec3(toPos.x + size, toPos.y, toPos.z), color);
    AddLine(category,
            glm::vec3(toPos.x, toPos.y - size, toPos.z), glm::vec3(toPos.x + size, toPos.y - size, toPos.z),
            color);


    //// Now all the y values
    //// Up
    //normal = glm::normalize(center - glm::vec3(center.x, center.y + halfSize, center.z));
    //fromPos = center + glm::vec3(-halfSize, halfSize, -halfSize);// Move half to left, up and back
    //toPos = fromPos + glm::vec3(size, 0, size);                     // Move fully back
    //AddRectangle(category, fromPos, toPos, normal, color);
    //
    //// Down
    //normal = glm::normalize(center - glm::vec3(center.x, center.y - halfSize, center.z));
    //fromPos.y -= size;
    //toPos.y -= size;
    //AddRectangle(category, fromPos, toPos, normal, color);

    //// Lastly, the z values
    //// Forward
    //normal = glm::normalize(center - glm::vec3(center.x, center.y, center.z + halfSize));
    //fromPos = center + glm::vec3(-halfSize, -halfSize, halfSize);  // Move half to left, down and forward
    //toPos = fromPos + glm::vec3(size, size, 0);                    // Move fully back
    //AddRectangle(category, fromPos, toPos, normal, color);

    //// Back
    //normal = glm::normalize(center - glm::vec3(center.x, center.y, center.z - halfSize));
    //fromPos.z -= size;
    //toPos.z -= size;
    //AddRectangle(category, fromPos, toPos, normal, color);
}

void bee::DebugRenderer::AddFilledVoxel(DebugCategory::Enum category,
                                        const glm::vec3& center,
                                        float size,
                                        const glm::vec4& color)
{
    // Add Square on all 6 sides

    // First add left and right plane (x values)

    float halfSize = size * 0.5f;

    // Right
    glm::vec3 targetPos = glm::vec3(center.x + halfSize, center.y, center.z);
    glm::vec3 normal = glm::normalize(center - targetPos);
    AddFilledSquare(category, targetPos, size, normal, color);

    // Left
    targetPos.x -= size;
    normal = glm::normalize(center - targetPos);
    AddFilledSquare(category, targetPos, size, normal, color);

    // Up
    targetPos = glm::vec3(center.x, center.y + halfSize, center.z);
    normal = glm::normalize(center - targetPos);
    AddFilledSquare(category, targetPos, size, normal, color);

    // Down
    targetPos.y -= size;
    normal = glm::normalize(center - targetPos);
    AddFilledSquare(category, targetPos, size, normal, color);

    // Forward
    targetPos = glm::vec3(center.x, center.y, center.z + halfSize);
    normal = glm::normalize(center - targetPos);
    AddFilledSquare(category, targetPos, size, normal, color);

    // Backward
    targetPos.z -= size;
    normal = glm::normalize(center - targetPos);
    AddFilledSquare(category, targetPos, size, normal, color);

}
