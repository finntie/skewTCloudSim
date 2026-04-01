#pragma once

#include <glm/glm.hpp>
#include <memory>

namespace bee
{

/// <summary>
/// A set of categories to turn on/off debug rendering.
/// </summary>
struct DebugCategory
{
    enum Enum : unsigned int
    {
        General = 1 << 0,
        Gameplay = 1 << 1,
        Physics = 1 << 2,
        Sound = 1 << 3,
        Rendering = 1 << 4,
        AINavigation = 1 << 5,
        AIDecision = 1 << 6,
        Editor = 1 << 7,
        AccelStructs = 1 << 8,
        Grid = 1 << 9,
        All = 0xFFFFFFFF
    };
};

class colorScheme;
/// <summary>
/// Renders debug lines and shapes. It can be called from any place in the code.
/// </summary>
class DebugRenderer
{
public:
    DebugRenderer();
    ~DebugRenderer();

    /// <summary>
    /// Initialize the debug renderer.
    /// </summary>
    void Render();

    /// <summary>
    /// Add a line to be rendered.
    /// </summary>
    void AddLine(DebugCategory::Enum category,
                 const glm::vec3& from,
                 const glm::vec3& to,
                 const glm::vec4& color);  // Implemented per platform.

    /// <summary>
    /// Add a triangle to be rendered.
    /// </summary>
    void AddTriangle(DebugCategory::Enum category,
                 const glm::vec3& first,
                 const glm::vec3& second,
                 const glm::vec3& third,
                 const glm::vec4& color);  // Implemented per platform.

    /// <summary>
    /// Add a circle to be rendered.
    /// </summary>
    void AddCircle(DebugCategory::Enum category,
                   const glm::vec3& center,
                   float radius,
                   const glm::vec3& normal,
                   const glm::vec4& color);  // Implemented for all platforms.

    /// <summary>
    /// Add a square to be rendered.
    /// </summary>
    void AddSquare(DebugCategory::Enum category,
                   const glm::vec3& center,
                   float size,
                   const glm::vec3& normal,
                   const glm::vec4& color);  // Implemented for all platforms.

    /// <summary>
    /// Add a filled square to be rendered.
    /// </summary>
    void AddFilledSquare(DebugCategory::Enum category,
                   const glm::vec3& center,
                   float size,
                   const glm::vec3& normal,
                   const glm::vec4& color);  // Implemented for all platforms.

    /// <summary>
    /// Add a rectangle to be rendered.
    /// </summary>
    void AddRectangle(DebugCategory::Enum category,
                         const glm::vec3& from,
                         const glm::vec3& to,
                         const glm::vec3& normal,
                         const glm::vec4& color);  // Implemented for all platforms.

    /// <summary>
    /// Add a cylinder to be rendered.
    /// </summary>
    void AddCylinder(DebugCategory::Enum category,
                     const glm::vec3& center1,
                     const glm::vec3& center2,
                     float radius,
                     const glm::vec4& color);

    /// <summary>
    /// Add an arrow to be rendered
    /// </summary>
    void AddArrow(DebugCategory::Enum category,
                  const glm::vec3& center,
                  const glm::vec3& normal,
                  const glm::vec3& pointDir,
                  float size,
                  const glm::vec4& color);

    /// <summary>
    /// Add a voxel to be rendered
    /// </summary>
    void AddVoxel(DebugCategory::Enum category,
                  const glm::vec3& center,
                  float size,
                  const glm::vec4& color);

    /// <summary>
    /// Turn on/off the debug renderer per category.
    /// </summary>
    void SetCategoryFlags(unsigned int flags) { m_categoryFlags = flags; }

    /// <summary>
    /// Get the current category flags.
    /// </summary>
    unsigned int GetCategoryFlags() const { return m_categoryFlags; }

    colorScheme& GetColorScheme() { return *m_colorSchemeObj; }

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
    unsigned int m_categoryFlags;
    std::unique_ptr<colorScheme> m_colorSchemeObj = nullptr;
};

}  // namespace bee
