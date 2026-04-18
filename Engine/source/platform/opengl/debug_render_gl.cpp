#include "rendering/debug_render.hpp"
#include <glm/gtc/constants.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "tools/log.hpp"
#include "core/transform.hpp"
#include "rendering/render_components.hpp"
#include "core/engine.hpp"
#include "core/ecs.hpp"
#include "platform/opengl/shader_gl.hpp"
#include "platform/opengl/open_gl.hpp"
#include "rendering/colors.hpp"

using namespace bee;
using namespace glm;

class bee::DebugRenderer::Impl
{
public:
    Impl();
    ~Impl();
    bool AddLine(const vec3& from, const vec3& to, const vec4& color);
    bool AddTriangle(const vec3& first, const vec3& second, const vec3& last, const vec4& color);
    void Render(const mat4& view, const mat4& projection);

    static int const m_maxLines = 32760 * 4 * 4;
    static int const m_maxTriangles = 32760 * 4 * 4;

    int m_linesCount = 0;
    int m_trianglesCount = 0;
    struct VertexPosition3DColor
    {
        glm::vec3 Position;
        glm::vec4 Color;
    };
    VertexPosition3DColor* m_vertexArray = nullptr;
    VertexPosition3DColor* m_vertexTriangleArray = nullptr;
    unsigned int debug_program = 0;
    unsigned int m_linesVAO = 0;
    unsigned int m_linesVBO = 0;
    unsigned int m_trianglesVAO = 0;
    unsigned int m_trianglesVBO = 0;
};

bee::DebugRenderer::DebugRenderer()
{
    m_categoryFlags = DebugCategory::General | DebugCategory::Gameplay | DebugCategory::Physics | DebugCategory::Rendering |
                      DebugCategory::AINavigation | DebugCategory::AIDecision | DebugCategory::Editor;

    m_impl = std::make_unique<Impl>();
    m_colorSchemeObj = std::make_unique<colorScheme>();
}

DebugRenderer::~DebugRenderer() = default;

void DebugRenderer::Render()
{
    const int n = 20;
    const float size = 20.0f;
    const float step = size / (float)n;
    if (m_categoryFlags & DebugCategory::Grid)
    {
        // Render grid
        for (int i = -n; i <= n; i++)
        {
            auto color = i == 0 ? vec4(1.0f) : vec4(0.5f);
            AddLine(DebugCategory::Grid, vec3(-size, 0.0f, step * i), vec3(size, 0.0f, step * i), color);
            AddLine(DebugCategory::Grid, vec3(step * i, 0.0f, -size), vec3(step * i, 0.0f, size), color);
        }
    }

    for (const auto& [entity, transform, camera] : Engine.ECS().Registry.view<Transform, Camera>().each())
    {
        // Get the view and projection matrices from the camera
        m_impl->Render(inverse(transform.World()), camera.Projection);
    }
}

void DebugRenderer::AddLine(DebugCategory::Enum category, const vec3& from, const vec3& to, const vec4& color)
{
    if (!(m_categoryFlags & category)) return;
    m_impl->AddLine(from, to, color);
}

void bee::DebugRenderer::AddTriangle(DebugCategory::Enum category,
                                     const glm::vec3& first,
                                     const glm::vec3& second,
                                     const glm::vec3& third,
                                     const glm::vec4& color)
{
    if (!(m_categoryFlags & category)) return;
    m_impl->AddTriangle(first, second, third, color);
}

bee::DebugRenderer::Impl::Impl()
{
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

    GLuint vertShader = 0;
    GLuint fragShader = 0;
    GLboolean res = GL_FALSE;

    debug_program = glCreateProgram();
    LabelGL(GL_PROGRAM, debug_program, "Debug Renderer Program");

    res = Shader::CompileShader(&vertShader, GL_VERTEX_SHADER, vsSource);
    if (!res)
    {
        Log::Error("DebugRenderer failed to compile vertex shader");
        return;
    }

    res = Shader::CompileShader(&fragShader, GL_FRAGMENT_SHADER, fsSource);
    if (!res)
    {
        Log::Error("DebugRenderer failed to compile fragment shader");
        return;
    }

    glAttachShader(debug_program, vertShader);
    glAttachShader(debug_program, fragShader);

    if (!Shader::LinkProgram(debug_program))
    {
        glDeleteShader(vertShader);
        glDeleteShader(fragShader);
        glDeleteProgram(debug_program);
        Log::Error("DebugRenderer failed to link shader program");
        return;
    }

    glDeleteShader(vertShader);
    glDeleteShader(fragShader);

    //--------Triangles-------
    glCreateVertexArrays(1, &m_trianglesVAO);
    glBindVertexArray(m_trianglesVAO);
    LabelGL(GL_VERTEX_ARRAY, m_trianglesVAO, "Debug Triangles VAO");

    // Allocate VBO
    glGenBuffers(1, &m_trianglesVBO);
    glBindBuffer(GL_ARRAY_BUFFER, m_trianglesVBO);
    LabelGL(GL_BUFFER, m_trianglesVBO, "Debug Triangles VBO");

    // Allocate into VBO
    const auto sizeTri = sizeof(m_vertexTriangleArray);
    glBufferData(GL_ARRAY_BUFFER, sizeTri, &m_vertexTriangleArray[0], GL_STREAM_DRAW);

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

    glBindVertexArray(0);  // TODO: Only do this when validating OpenGL

    //----------Lines--------
    glCreateVertexArrays(1, &m_linesVAO);
    glBindVertexArray(m_linesVAO);
    LabelGL(GL_VERTEX_ARRAY, m_linesVAO, "Debug Lines VAO");

    // Allocate VBO
    glGenBuffers(1, &m_linesVBO);
    glBindBuffer(GL_ARRAY_BUFFER, m_linesVBO);
    LabelGL(GL_BUFFER, m_linesVBO, "Debug Lines VBO");

    // Allocate into VBO
    const auto sizeLine = sizeof(m_vertexArray);
    glBufferData(GL_ARRAY_BUFFER, sizeLine, &m_vertexArray[0], GL_STREAM_DRAW);

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

    glBindVertexArray(0);  // TODO: Only do this when validating OpenGL
}

bee::DebugRenderer::Impl::~Impl()
{
    delete[] m_vertexArray;
    delete[] m_vertexTriangleArray;
    glDeleteVertexArrays(1, &m_linesVAO);
    glDeleteBuffers(1, &m_linesVBO);
    glDeleteVertexArrays(1, &m_trianglesVAO);
    glDeleteBuffers(1, &m_trianglesVBO);
}

bool bee::DebugRenderer::Impl::AddLine(const vec3& from, const vec3& to, const vec4& color)
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

bool bee::DebugRenderer::Impl::AddTriangle(const vec3& first, const vec3& second, const vec3& last, const vec4& color)
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

void bee::DebugRenderer::Impl::Render(const mat4& view, const mat4& projection)
{
    // Enable depth testing
    glEnable(GL_DEPTH_TEST);

    // Clear the depth buffer (optional but recommended for each frame)
    glClear(GL_DEPTH_BUFFER_BIT);


    glm::mat4 vp = projection * view;
    glUseProgram(debug_program);
    glUniformMatrix4fv(1, 1, false, value_ptr(vp));

    // Render debug lines
    glBindVertexArray(m_linesVAO);

    glDepthMask(GL_TRUE);  // Disable depth writing for lines (they shouldn’t affect depth buffer)
    if (m_linesCount > 0)
    {
        glBindBuffer(GL_ARRAY_BUFFER, m_linesVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(VertexPosition3DColor) * (m_maxLines * 2), &m_vertexArray[0], GL_DYNAMIC_DRAW);
        glDrawArrays(GL_LINES, 0, m_linesCount * 2);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    m_linesCount = 0;


    // Render debug triangles
    glBindVertexArray(m_trianglesVAO);

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
}