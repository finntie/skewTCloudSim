#pragma once
#include <glm/glm.hpp>

class newShader;

class shapesGL
{
public:
    shapesGL();
    ~shapesGL();
    bool AddLine(const glm::vec3& from, const glm::vec3& to, const glm::vec4& color);
    bool AddTriangle(const glm::vec3& first, const glm::vec3& second, const glm::vec3& last, const glm::vec4& color);

     /// <summary>
    /// Add a circle to be rendered.
    /// </summary>
    void AddCircle(const glm::vec3& center, float radius, const glm::vec3& normal, const glm::vec4& color);

    /// <summary>
    /// Add a square to be rendered.
    /// </summary>
    void AddSquare(const glm::vec3& center, float size, const glm::vec3& normal, const glm::vec4& color);

    /// <summary>
    /// Add a filled square to be rendered.
    /// </summary>
    void AddFilledSquare(const glm::vec3& center, float size, const glm::vec3& normal, const glm::vec4& color);

    /// <summary>
    /// Add a rectangle to be rendered.
    /// </summary>
    void AddRectangle(const glm::vec3& from, const glm::vec3& to, const glm::vec3& normal, const glm::vec4& color);

    /// <summary>
    /// Add a cylinder to be rendered.
    /// </summary>
    void AddCylinder(const glm::vec3& center1, const glm::vec3& center2, float radius, const glm::vec4& color);

    /// <summary>
    /// Add an arrow to be rendered
    /// </summary>
    void AddArrow(const glm::vec3& center,
                  const glm::vec3& normal,
                  const glm::vec3& pointDir,
                  float size,
                  const glm::vec4& color);

    /// <summary>
    /// Add a voxel to be rendered
    /// </summary>
    void AddVoxel(const glm::vec3& center, float size, const glm::vec4& color);

    /// <summary>
    /// Add a filled voxel to be rendered
    /// </summary>
    void AddFilledVoxel(const glm::vec3& center, float size, const glm::vec4& color);

    /// <summary>
    /// Renders all lines and triangles listed beforehand, input view and projection of the camera
    /// </summary>
    void render(glm::mat4& view, glm::mat4& projection);


private:

    static int const m_maxLines = 32760 * 4;
    static int const m_maxTriangles = 32760 * 4;

    int m_linesCount = 0;
    int m_trianglesCount = 0;
    struct VertexPosition3DColor
    {
        glm::vec3 Position;
        glm::vec4 Color;
    };
    VertexPosition3DColor* m_vertexArray = nullptr;
    VertexPosition3DColor* m_vertexTriangleArray = nullptr;
    newShader* m_shader;

    unsigned int m_linesVAO = 0;
    unsigned int m_linesVBO = 0;
    unsigned int m_trianglesVAO = 0;
    unsigned int m_trianglesVBO = 0;
    unsigned int m_frameBuffer = 0;
};


