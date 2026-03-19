#pragma once
#include <string>
#include <glm/glm.hpp>

namespace bee
{
    //Using same style as debugrenderer

class DrawImage
{
public:

    DrawImage();
    ~DrawImage();
    bool AddLine(const glm::vec3& from, const glm::vec3& to, const glm::vec4& color);
    bool AddTriangle(const glm::vec3& first, const glm::vec3& second, const glm::vec3& last, const glm::vec4& color);

    //Draw all created lines within from-to distance, onto a texture.
    //If texture = 0, it will create a new texture and set its ID, else it will use the set ID.
    void getDrawnImage(const glm::vec2& from, const glm::vec2& to, unsigned int& texture);

private:

    void drawLines(const glm::vec2& from, const glm::vec2& to);

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
    unsigned int draw_image_program = 0;
    unsigned int m_linesVAO = 0;
    unsigned int m_linesVBO = 0;
    unsigned int m_trianglesVAO = 0;
    unsigned int m_trianglesVBO = 0;
    unsigned int m_frameBuffer = 0;
};

}  // namespace bee