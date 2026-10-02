#include "outside/rendering/shapes_gl.hpp"

#include "outside/rendering/new_open_gl.hpp"
#include "outside/rendering/helper_open_gl.hpp"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

shapesGL::shapesGL() 
{
    m_shader = new newShader();

    m_vertexArray = new VertexPosition3DColor[m_maxLines * 2];
    m_vertexTriangleArray = new VertexPosition3DColor[m_maxTriangles * 3];

    const auto* const vsSource =
        "#version 460 core												\n\
		layout (location = 1) in vec3 a_position;						\n\
		layout (location = 2) in vec4 a_color;							\n\
		layout (location = 1) uniform mat4 u_worldviewproj;				\n\
		out vec4 v_color;												\n\
																		\n\
		void main()														\n\
		{																\n\
			v_color = a_color;											\n\
			gl_Position = u_worldviewproj * vec4(a_position, 1.0);		\n\
		}";

    const auto* const fsSource =
        "#version 460 core												\n\
		in vec4 v_color;												\n\
		out vec4 frag_color;											\n\
																		\n\
		void main()														\n\
		{																\n\
			frag_color = v_color;										\n\
		}";

    m_shader->loadShader(vsSource, fsSource);

	    //----Triangles----
    glCreateVertexArrays(1, &m_trianglesVAO);
    glBindVertexArray(m_trianglesVAO);

    glGenBuffers(1, &m_trianglesVBO);
    glBindBuffer(GL_ARRAY_BUFFER, m_trianglesVBO);

    //--Alocate into VBO--
    const unsigned int triSize = sizeof(m_vertexTriangleArray);
    glBufferData(GL_ARRAY_BUFFER, triSize, &m_vertexTriangleArray[0], GL_STREAM_DRAW);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,
                          3,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(VertexPosition3DColor),
                          reinterpret_cast<void*>(offsetof(VertexPosition3DColor, Position)));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2,
                          4,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(VertexPosition3DColor),
                          reinterpret_cast<void*>(offsetof(VertexPosition3DColor, Color)));

    glBindVertexArray(0);

    //----Lines----
    glCreateVertexArrays(1, &m_linesVAO);
    glBindVertexArray(m_linesVAO);

    glGenBuffers(1, &m_linesVBO);
    glBindBuffer(GL_ARRAY_BUFFER, m_linesVBO);

    //--Alocate into VBO--
    const unsigned int linesSize = sizeof(m_vertexArray);
    glBufferData(GL_ARRAY_BUFFER, linesSize, &m_vertexArray[0], GL_STREAM_DRAW);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,
                          3,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(VertexPosition3DColor),
                          reinterpret_cast<void*>(offsetof(VertexPosition3DColor, Position)));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2,
                          4,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(VertexPosition3DColor),
                          reinterpret_cast<void*>(offsetof(VertexPosition3DColor, Color)));

    glBindVertexArray(0);
}

shapesGL::~shapesGL() 
{
    delete m_shader;
    delete[] m_vertexArray;
    delete[] m_vertexTriangleArray;
    glDeleteVertexArrays(1, &m_linesVAO);
    glDeleteBuffers(1, &m_linesVBO);
    glDeleteVertexArrays(1, &m_trianglesVAO);
    glDeleteBuffers(1, &m_trianglesVBO);

}

bool shapesGL::AddLine(const glm::vec3& from, const glm::vec3& to, const glm::vec4& color) 
{ 
    if (m_linesCount < m_maxLines)
    {
        m_vertexArray[m_linesCount * 2].Position = from;
        m_vertexArray[m_linesCount * 2 + 1].Position = to;
        m_vertexArray[m_linesCount * 2].Color = color;
        m_vertexArray[m_linesCount * 2 + 1].Color = color;
        ++m_linesCount;
        return true;
    }
    return false;
}

bool shapesGL::AddTriangle(const glm::vec3& first, const glm::vec3& second, const glm::vec3& last, const glm::vec4& color)
{
    if (m_trianglesCount < m_maxTriangles)
    {
        m_vertexTriangleArray[m_trianglesCount * 3].Position = first;
        m_vertexTriangleArray[m_trianglesCount * 3 + 1].Position = second;
        m_vertexTriangleArray[m_trianglesCount * 3 + 2].Position = last;
        m_vertexTriangleArray[m_trianglesCount * 3].Color = color;
        m_vertexTriangleArray[m_trianglesCount * 3 + 1].Color = color;
        m_vertexTriangleArray[m_trianglesCount * 3 + 2].Color = color;
        ++m_trianglesCount;
        return true;
    }
    return false;
}

void shapesGL::AddCircle(const glm::vec3& center, float radius, const glm::vec3& normal, const glm::vec4& color)
{
    constexpr float dt = glm::two_pi<float>() / 32.0f;
    float t = 0.0f;

    const auto& rotation = glm::rotation(glm::vec3(0, 0, 1), normal);

    glm::vec3 v0 = center + radius * glm::rotate(rotation, glm::vec3(cos(t), sin(t), 0));
    for (; t < glm::two_pi<float>(); t += dt)
    {
        glm::vec3 v1 = center + radius * glm::rotate(rotation, glm::vec3(cos(t + dt), sin(t + dt), 0));
        AddLine(v0, v1, color);
        v0 = v1;
    }
}

void shapesGL::AddSquare(const glm::vec3& center, float size, const glm::vec3& normal, const glm::vec4& color)
{
    const auto& rotation = glm::rotation(glm::vec3(0, 0, 1), normal);

    const float s = size * 0.5f;
    auto A = center + glm::rotate(rotation, glm::vec3(-s, -s, 0.0f));
    auto B = center + glm::rotate(rotation, glm::vec3(-s, s, 0.0f));
    auto C = center + glm::rotate(rotation, glm::vec3(s, s, 0.0f));
    auto D = center + glm::rotate(rotation, glm::vec3(s, -s, 0.0f));

    // TODO: use normal

    AddLine(A, B, color);
    AddLine(B, C, color);
    AddLine(C, D, color);
    AddLine(D, A, color);
}

void shapesGL::AddFilledSquare(const glm::vec3& center, float size, const glm::vec3& normal, const glm::vec4& color)
{
    const auto& rotation = glm::rotation(glm::vec3(0, 0, 1), normal);

    const float s = size * 0.5f;
    auto A = center + glm::rotate(rotation, glm::vec3(-s, -s, 0.0f));
    auto B = center + glm::rotate(rotation, glm::vec3(-s, s, 0.0f));
    auto C = center + glm::rotate(rotation, glm::vec3(s, s, 0.0f));
    auto D = center + glm::rotate(rotation, glm::vec3(s, -s, 0.0f));

    // TODO: use normal

    AddTriangle(A, B, C, color);
    AddTriangle(C, D, A, color);
}

void shapesGL::AddRectangle(const glm::vec3& from, const glm::vec3& to, const glm::vec3& normal, const glm::vec4& color)
{
    const glm::vec3 diagonal = to - from;
    const glm::vec3 middle = from + diagonal * 0.5f;
    const glm::vec3 side = glm::cross(normal, diagonal) * 0.5f;

    const glm::vec3 A = from;
    const glm::vec3 B = middle + side;
    const glm::vec3 C = to;
    const glm::vec3 D = middle - side;

    AddLine(A, B, color);
    AddLine(B, C, color);
    AddLine(C, D, color);
    AddLine(D, A, color);
}

void shapesGL::AddCylinder(const glm::vec3& center1, const glm::vec3& center2, float radius, const glm::vec4& color)
{
    constexpr float dt = glm::two_pi<float>() / 16.0f;
    float t = 0.0f;

    const auto& diff = center2 - center1;
    const auto& rotation = glm::rotation(glm::vec3(0, 0, 1), glm::normalize(diff));

    glm::vec3 v0 = center1 + radius * glm::rotate(rotation, glm::vec3(cos(t), sin(t), 0));
    for (; t < glm::two_pi<float>(); t += dt)
    {
        glm::vec3 v1 = center1 + radius * glm::rotate(rotation, glm::vec3(cos(t + dt), sin(t + dt), 0));
        AddLine(v0, v1, color);
        AddLine(v0 + diff, v1 + diff, color);
        AddLine(v0, v0 + diff, color);
        v0 = v1;
    }
}

void shapesGL::AddArrow(const glm::vec3& center,
                        const glm::vec3& normal,
                        const glm::vec3& pointDir,
                        float size,
                        const glm::vec4& color)
{
    const glm::vec3 forward = glm::normalize(pointDir);
    const glm::vec3 side = glm::normalize(glm::cross(normal, forward));

    const float s = size * 0.5f;

    auto A = center - forward * s;  // tail
    auto B = center + forward * s;  // tip

    auto C = B - forward * (s * 0.5f) + side * (s * 0.5f);
    auto D = B - forward * (s * 0.5f) - side * (s * 0.5f);

    AddLine(A, B, color);
    AddLine(B, C, color);
    AddLine(B, D, color);
}

void shapesGL::AddVoxel(const glm::vec3& center, float size, const glm::vec4& color)
{
    float halfSize = size * 0.5f;

    // Add forward rectangle
    glm::vec3 normal = glm::normalize(center - glm::vec3(center.x + halfSize, center.y, center.z));
    glm::vec3 fromPos = center + glm::vec3(halfSize, -halfSize, -halfSize);  // Move half to right, back and down
    glm::vec3 toPos = fromPos + glm::vec3(0, size, size);                    // Move fully back
    AddRectangle(fromPos, toPos, normal, color);

    // Add backward rectangle
    normal = glm::normalize(center - glm::vec3(center.x - halfSize, center.y, center.z));
    fromPos.x -= size;
    toPos.x -= size;
    AddRectangle(fromPos, toPos, normal, color);

    // Connect the rectangles with 4 lines
    AddLine(fromPos, glm::vec3(fromPos.x + size, fromPos.y, fromPos.z), color);
    AddLine(glm::vec3(fromPos.x, fromPos.y + size, fromPos.z), glm::vec3(fromPos.x + size, fromPos.y + size, fromPos.z), color);
    AddLine(toPos, glm::vec3(toPos.x + size, toPos.y, toPos.z), color);
    AddLine(glm::vec3(toPos.x, toPos.y - size, toPos.z), glm::vec3(toPos.x + size, toPos.y - size, toPos.z), color);
}

void shapesGL::AddFilledVoxel(const glm::vec3& center, float size, const glm::vec4& color)
{
    // Add Square on all 6 sides

    // First add left and right plane (x values)

    float halfSize = size * 0.5f;

    // Right
    glm::vec3 targetPos = glm::vec3(center.x + halfSize, center.y, center.z);
    glm::vec3 normal = glm::normalize(center - targetPos);
    AddFilledSquare(targetPos, size, normal, color);

    // Left
    targetPos.x -= size;
    normal = glm::normalize(center - targetPos);
    AddFilledSquare(targetPos, size, normal, color);

    // Up
    targetPos = glm::vec3(center.x, center.y + halfSize, center.z);
    normal = glm::normalize(center - targetPos);
    AddFilledSquare(targetPos, size, normal, color);

    // Down
    targetPos.y -= size;
    normal = glm::normalize(center - targetPos);
    AddFilledSquare(targetPos, size, normal, color);

    // Forward
    targetPos = glm::vec3(center.x, center.y, center.z + halfSize);
    normal = glm::normalize(center - targetPos);
    AddFilledSquare(targetPos, size, normal, color);

    // Backward
    targetPos.z -= size;
    normal = glm::normalize(center - targetPos);
    AddFilledSquare(targetPos, size, normal, color);
}

void shapesGL::render(glm::mat4& view, glm::mat4& projection) 
{ 
    glEnable(GL_DEPTH_TEST); 
    glClear(GL_DEPTH_BUFFER_BIT);


    glm::mat4 vp = projection * view;
    m_shader->useProgram();

    glUniformMatrix4fv(1, 1, false, glm::value_ptr(vp));

    glBindVertexArray(m_linesVAO);

    glDepthMask(GL_TRUE);
    if (m_linesCount > 0)
    {
        glBindBuffer(GL_ARRAY_BUFFER, m_linesVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(VertexPosition3DColor) * (m_maxLines * 2), &m_vertexArray[0], GL_DYNAMIC_DRAW);
        glDrawArrays(GL_LINES, 0, m_linesCount * 2);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    m_linesCount = 0;


    glBindVertexArray(m_trianglesVAO);

    glDepthMask(GL_TRUE);
    if (m_trianglesCount > 0)
    {
        glBindBuffer(GL_ARRAY_BUFFER, m_trianglesVBO);
        glBufferData(GL_ARRAY_BUFFER,
                     sizeof(VertexPosition3DColor) * (m_maxTriangles * 3),
                     &m_vertexTriangleArray[0],
                     GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, m_trianglesCount * 3);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    m_trianglesCount = 0;
    glBindVertexArray(0);

    m_shader->unuseProgram();
}
