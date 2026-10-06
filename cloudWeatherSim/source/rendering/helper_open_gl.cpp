#include "rendering/helper_open_gl.hpp"

#include "rendering/new_open_gl.hpp"


#include <fstream>
#include <sstream>


newShader::~newShader() 
{
    if (m_program > 0)
    {
        glDeleteProgram(m_program);
        m_program = 0;
    }
}

unsigned int newShader::loadShader(const char* vertexShader, const char* fragmentShader)
{ 
    int success;
    char infoLog[512];

    // Bind vertex shader and compile
    GLuint vertShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertShader, 1, &vertexShader, NULL);
    glCompileShader(vertShader);

    // Check for errors
    glGetShaderiv(vertShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertShader, 512, NULL, infoLog);
        printf("OPENGL ERROR: vertex shader compilation failed: %s\n", infoLog);
        return 0;
    }

    GLuint fragShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragShader, 1, &fragmentShader, NULL);
    glCompileShader(fragShader);

    // Check for errors
    glGetShaderiv(fragShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragShader, 512, NULL, infoLog);
        printf("OPENGL ERROR: fragment shader compilation failed: %s\n", infoLog);
        return 0;
    }

    // link shaders
    GLuint program = 0;
    program = glCreateProgram();
    glAttachShader(program, vertShader);
    glAttachShader(program, fragShader);
    glLinkProgram(program);

    // Check for errors
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        printf("OPENGL ERROR: shader program linking failed: %s\n", infoLog);
        return 0;
    }

    // I don't need you anymore
    glDeleteShader(vertShader);
    glDeleteShader(fragShader);

    m_vertexShader = vertexShader;
    m_fragmentShader = fragmentShader;
    m_program = program;

    return program;
}


unsigned int newShader::loadShaderFromFile(const char* vertexShaderFile, const char* fragmentShaderFile) 
{

	std::ifstream vertexFile(vertexShaderFile);
    std::ifstream fragmentFile(fragmentShaderFile);
    std::stringstream vertexBuffer;
    std::stringstream fragmentBuffer;
    vertexBuffer << vertexFile.rdbuf();
    fragmentBuffer << fragmentFile.rdbuf();
    std::string vertexString = vertexBuffer.str();
    std::string fragmentString = fragmentBuffer.str();

    return loadShader(vertexString.c_str(), fragmentString.c_str());
}

void newShader::useProgram() 
{ 
    glUseProgram(m_program); 
}

void newShader::unuseProgram() 
{
    glUseProgram(0); 
}

// Renders a 1x1 XY quad in NDC, Copied from BEE engine
void renderQuad()
{
    static unsigned int quadVAO = 0;
    static unsigned int quadVBO = 0;

    if (quadVAO == 0)
    {
        float quadVertices[] = {
            // positions        // texture coordinates
            -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
            1.0f,  1.0f, 0.0f, 1.0f, 1.0f, 1.0f,  -1.0f, 0.0f, 1.0f, 0.0f,
        };
        // setup plane VAO
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        // Linter warning "modernize-use-nullptr" does not make sense for this use case; we truly mean the value 0 here
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);  // NOLINT(modernize-use-nullptr)
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    }
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}