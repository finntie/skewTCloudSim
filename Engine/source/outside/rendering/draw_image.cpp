#include "outside/rendering/draw_image.hpp"

#include "outside/rendering/new_open_gl.hpp"
#include "outside/rendering/helper_open_gl.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

DrawImage::DrawImage() 
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

    //And, donzo.
}

DrawImage::~DrawImage()
{
    delete m_shader;
    delete[] m_vertexArray;
    delete[] m_vertexTriangleArray;
    glDeleteVertexArrays(1, &m_linesVAO);
    glDeleteBuffers(1, &m_linesVBO);
    glDeleteVertexArrays(1, &m_trianglesVAO);
    glDeleteBuffers(1, &m_trianglesVBO);
}

bool DrawImage::AddLine(const glm::vec3& from, const glm::vec3& to, const glm::vec4& color) 
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

bool DrawImage::AddTriangle(const glm::vec3& first, const glm::vec3& second, const glm::vec3& last, const glm::vec4& color)
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

void DrawImage::getDrawnImage(const glm::vec2& from, const glm::vec2& to, unsigned int& texture) 
{ 
    if (texture == 0)
    {
        //Create and bind buffer and texture
        glGenFramebuffers(1, &m_frameBuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, m_frameBuffer);

        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);

        // Create empty image
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 512, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
        glViewport(0, 0, 512, 512);
        glClearColor(0.35f, 0.55f, 0.9f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
    else
    {
        //Bind buffer and texture
        glBindFramebuffer(GL_FRAMEBUFFER, m_frameBuffer);
        glBindTexture(GL_TEXTURE_2D, texture);
        //Clear
        glViewport(0, 0, 512, 512);
        glClearColor(0.35f, 0.55f, 0.9f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    //Draw into the texture
    drawLines(from, to);

    //Reset buffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void DrawImage::drawLines(const glm::vec2& from, const glm::vec2& to)
{
    //TODO: could store on heap
    glm::mat4 orthoMat = glm::ortho(from.x, to.x, from.y, to.y, 0.1f, 100.0f);
    glm::mat4 view = inverse(glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, 50)));
    glm::mat4 vp = orthoMat * view;

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    m_shader->useProgram();
    glUniformMatrix4fv(1, 1, false, glm::value_ptr(vp));

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
    m_shader->unuseProgram();
}
