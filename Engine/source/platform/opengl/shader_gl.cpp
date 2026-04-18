#include "platform/opengl/shader_gl.hpp"

#include <glm/gtc/type_ptr.hpp>

#include "platform/opengl/image_gl.hpp"
#include "platform/opengl/open_gl.hpp"
#include "tools/log.hpp"
#include "tools/shader_preprocessor.hpp"

using namespace std;
using namespace bee;

////////////////////////////////////////////////////////////////////////////////
//
//                              ShaderParameter
//
////////////////////////////////////////////////////////////////////////////////

void ShaderParameter::SetValue(float val) const
{
    if (!IsValid()) return;
    assert(m_type == GL_FLOAT);
    glUniform1f(m_location, val);
}

void ShaderParameter::SetValue(int val) const
{
    if (!IsValid()) return;

    // assert(m_type == GL_INT);
    glUniform1i(m_location, val);
}

void ShaderParameter::SetValue(unsigned int val) const
{
    if (!IsValid()) return;
    assert(m_type == GL_UNSIGNED_INT);
    glUniform1ui(m_location, val);
}

void ShaderParameter::SetValue(bool val) const
{
    if (!IsValid()) return;
    assert(m_type == GL_BOOL);
    glUniform1i(m_location, val);
}

void ShaderParameter::SetValue(const glm::vec2& vec) const
{
    if (!IsValid()) return;
    assert(m_type == GL_FLOAT_VEC2);
    glUniform2fv(m_location, 1, &vec.x);
}

void ShaderParameter::SetValue(const glm::vec3& vec) const
{
    if (!IsValid()) return;
    assert(m_type == GL_FLOAT_VEC3);
    glUniform3fv(m_location, 1, &vec.x);
}

void ShaderParameter::SetValue(const glm::vec4& vec) const
{
    if (!IsValid()) return;

    // Colors can be represted as vec3 or vec4 in shaders
    if (m_type == GL_FLOAT_VEC4)
        glUniform4fv(m_location, 1, &vec.x);
    else if (m_type == GL_FLOAT_VEC3)
        glUniform3fv(m_location, 1, &vec.x);
    else
        assert(false);
}

void ShaderParameter::SetValue(const glm::mat4& mtx, bool transpose) const
{
    if (!IsValid()) return;
    assert(m_type == GL_FLOAT_MAT4);
    glUniformMatrix4fv(m_location, 1, transpose, glm::value_ptr(mtx));
}

void ShaderParameter::SetValue(const Image& image) const
{
    if (!IsValid()) return;

    assert(m_type == GL_SAMPLER_2D || m_type == GL_SAMPLER_2D_SHADOW);

    // Use texture with index sampler. GL_TEXTURE1 = GL_TEXTURE1+1 is always true
    glActiveTexture(GL_TEXTURE0 + m_sampler);

    // Work with this texture
    glBindTexture(GL_TEXTURE_2D, image.GetTextureId());

    // Set the sampler
    glUniform1i(m_location, m_sampler);
}

////////////////////////////////////////////////////////////////////////////////
//
//                          ShaderAttribute
//
////////////////////////////////////////////////////////////////////////////////

void ShaderAttribute::SetAttributePointer(GLint size,
                                          GLenum type,
                                          GLboolean normalized,
                                          GLsizei stride,
                                          const GLvoid* pointer) const
{
    if (!IsValid()) return;

    glEnableVertexAttribArray(_location);

    glVertexAttribPointer(_location,   // attribute
                          size,        // number of elements per vertex element
                          type,        // the type of each element
                          normalized,  // take our values as-is or normalize
                          stride,      // no extra data between each position
                          pointer      // offset of first element
    );
}

void ShaderAttribute::DisableAttributePointer() const
{
    if (!IsValid()) return;

    glDisableVertexAttribArray(_location);
}

////////////////////////////////////////////////////////////////////////////////
//
//                                  Shader
//
////////////////////////////////////////////////////////////////////////////////

Shader::Shader(FileIO::Directory directory, const std::string& vertexFilename, const std::string& fragmentFilename)
    : Shader(directory, vertexFilename, "", fragmentFilename)
{
}

Shader::Shader(FileIO::Directory directory,
               const std::string& vertexFilename,
               const std::string& geometryFilename,
               const std::string& fragmentFilename)
    : Resource(ResourceType::Shader),
      m_vertexFilename(vertexFilename),
      m_fragmentFilename(fragmentFilename),
      m_geometryFilename(geometryFilename),
      m_computeFilename("")
{
    m_directory = directory;
    m_resourcePath = geometryFilename.empty() ? vertexFilename + " " + fragmentFilename
                                              : vertexFilename + " " + geometryFilename + " " + fragmentFilename;
    Shader::Reload();
    LabelGL(GL_PROGRAM, m_program, m_resourcePath);
}

Shader::Shader(FileIO::Directory directory, const std::string& computeFilename)
    : Resource(ResourceType::Shader),
      m_vertexFilename(""),
      m_fragmentFilename(""),
      m_geometryFilename(""),
      m_computeFilename(computeFilename)
{
    m_directory = directory;
    m_resourcePath = computeFilename;
    Shader::Reload();
    LabelGL(GL_PROGRAM, m_program, m_resourcePath);
}

Shader::~Shader()
{
    if (m_program > 0)
    {
        glDeleteProgram(m_program);
        m_program = 0;
    }
}

GLuint Shader::GetProgram() const { return m_program; }

void Shader::Deactivate() { glUseProgram(0); }

void Shader::Reload()
{
    if (m_program > 0)
    {
        glDeleteProgram(m_program);
        m_program = 0;
    }

    bool success = Load();

    if (!success)
    {
        success = LoadMagentaShader();
        assert(success);
        LoadParameters();
    }

    if (!success)
    {
        Log::Error("Unable to load shader {}", m_resourcePath.c_str());
    }
    else
    {
        LoadParameters();
    }
}

void Shader::LoadParameters()
{
    // The shader should invalidate when reloading a new shader file
    // as some information can be old
    for (auto& itr : m_parameters) itr.second->Invalidate();
    for (auto& itr : m_attributes) itr.second->Invalidate();

    // Get the number of uniforms and resize the parameters collection accordingly
    GLint numActiveUniforms = 0;
    glGetProgramiv(m_program, GL_ACTIVE_UNIFORMS, &numActiveUniforms);

    // Get the maximum name length
    GLint maxUniformNameLength = 0;
    glGetProgramiv(m_program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxUniformNameLength);
    assert(maxUniformNameLength < 512);

    // vector<GLchar> uniformNameData(maxUniformNameLength);

    GLchar uniformNameData[512];

    // Go over all the uniforms
    int samplerCount = 0;
    for (int uniform = 0; uniform < numActiveUniforms; uniform++)
    {
        GLint arraySize = 0;
        GLenum type = 0;
        GLsizei actualLength = 0;
        glGetActiveUniform(m_program, uniform, maxUniformNameLength, &actualLength, &arraySize, &type, &uniformNameData[0]);
        string name(&uniformNameData[0], actualLength);

        const GLint location = glGetUniformLocation(m_program, name.c_str());

        auto id = std::hash<std::string>{}(name);
        auto itr = m_parameters.find(id);
        if (itr != m_parameters.end())
        {
            if (type == GL_SAMPLER_2D || type == GL_SAMPLER_CUBE || type == GL_SAMPLER_2D_SHADOW)
                itr->second->Reset(this, name, type, location, samplerCount++);
            else
                itr->second->Reset(this, name, type, location);
        }
        else
        {
            ShaderParameter* param = nullptr;
            if (type == GL_SAMPLER_2D || type == GL_SAMPLER_CUBE || type == GL_SAMPLER_2D_SHADOW)
                param = new ShaderParameter(this, name, type, location, samplerCount++);
            else
                param = new ShaderParameter(this, name, type, location);
            m_parameters[id] = unique_ptr<ShaderParameter>(param);
        }
    }

    GLint numActiveAttribs = 0;
    glGetProgramiv(m_program, GL_ACTIVE_ATTRIBUTES, &numActiveAttribs);

    GLint maxAttribNameLength = 0;
    glGetProgramiv(m_program, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &maxAttribNameLength);
    vector<GLchar> attribNameData(maxAttribNameLength);

    auto numAttribs = (GLuint)numActiveAttribs;
    for (GLuint attrib = 0; attrib < numAttribs; ++attrib)
    {
        GLint arraySize = 0;
        GLenum type = 0;
        GLsizei actualLength = 0;
        glGetActiveAttrib(m_program,
                          attrib,
                          (GLsizei)attribNameData.size(),
                          &actualLength,
                          &arraySize,
                          &type,
                          attribNameData.data());
        std::string name(static_cast<char*>(attribNameData.data()));
        GLint location = glGetAttribLocation(m_program, name.c_str());

        auto itr = m_attributes.find(name);
        if (itr != m_attributes.end())
        {
            itr->second->Reset(this, name, type, location);
        }
        else
        {
            auto* attribute = new ShaderAttribute(this, name, type, location);
            m_attributes[name] = unique_ptr<ShaderAttribute>(attribute);
        }
    }
}

ShaderParameter* Shader::GetParameter(const string& name)
{
    const auto id = std::hash<std::string>{}(name);
    return GetParameter(id);
}

ShaderParameter* Shader::GetParameter(unsigned long long nameId)
{
    // Try to find param
    auto itr = m_parameters.find(nameId);
    if (itr != m_parameters.end()) return itr->second.get();

    // Create and return a non-valid param that is stored in collection
    // in case it becomes valid after a reload
    auto* param = new ShaderParameter();
    m_parameters[nameId] = unique_ptr<ShaderParameter>(param);
    return param;
}

ShaderAttribute* Shader::GetAttribute(const string& name)
{
    // Try to find param
    auto itr = m_attributes.find(name);
    if (itr != m_attributes.end()) return itr->second.get();

    // Create and return a non-valid param that is stored in collection
    // in case it becomes valid after a reload
    auto* attrib = new ShaderAttribute();
    m_attributes[name] = unique_ptr<ShaderAttribute>(attrib);
    return attrib;
}

void Shader::Activate() const { glUseProgram(GetProgram()); }

bool Shader::Validate() const
{
#if defined(DEBUG)
    if (!ValidateProgram(m_program))
    {
        Log::Error("Failed to validate program: {}", m_program);

        return false;
    }
#endif
    return true;
}

bool Shader::Load()
{
    static ShaderPreprocessor preprocessor;

    string vertShaderSource = "";
    if (!m_vertexFilename.empty()) vertShaderSource = preprocessor.Read(m_directory, m_vertexFilename);

    string geomShaderSource = "";
    if (!m_geometryFilename.empty()) geomShaderSource = preprocessor.Read(m_directory, m_geometryFilename);

    string fragShaderSource = "";
    if (!m_fragmentFilename.empty()) fragShaderSource = preprocessor.Read(m_directory, m_fragmentFilename);

    string computeShaderSource = "";
    if (!m_computeFilename.empty()) computeShaderSource = preprocessor.Read(m_directory, m_computeFilename);

    return LoadSource(vertShaderSource, geomShaderSource, fragShaderSource, computeShaderSource);
}

bool Shader::LoadMagentaShader()
{
    string vertShaderSource =
        "#version 430 core\n \
		uniform mat4 u_projection; \
		uniform mat4 u_view; \
		uniform mat4 bee_model; \
		in vec3 a_position; \
		void main() \
		{ \
			gl_Position = u_projection * u_view * bee_model * vec4(a_position, 1.0); \
		}";
    string fragShaderSource =
        "#version 430 core\n \
		out vec4 fragColor; \
		void main() \
		{ \
			fragColor = vec4(1.0, 0.0, 1.0, 1.0); \
		}";

    return LoadSource(vertShaderSource, "", fragShaderSource, "");
}

vector<ShaderParameter*> Shader::GetAllParameters()
{
    vector<ShaderParameter*> params;
    params.reserve(m_parameters.size());

    for (const auto& itr : m_parameters) params.push_back(itr.second.get());

    return params;
}

bool Shader::LoadSource(const string& vertexShader,
                        const string& geometryShader,
                        const string& fragmentShader,
                        const string& computeShader)
{
    GLuint vertShader = 0;
    GLuint fragShader = 0;
    GLuint geomShader = 0;
    GLuint compShader = 0;

    m_program = glCreateProgram();

    // Temp result
    GLboolean res = 0;

    if (!vertexShader.empty())
    {
        res = CompileShader(&vertShader, GL_VERTEX_SHADER, vertexShader.c_str());
        if (!res)
        {
            Log::Error("Shader::Load() Failed to compile vertex shader. File: {}", m_vertexFilename.c_str());
            return false;
        }
    }

    // Geometry shader is optional
    if (!geometryShader.empty())
    {
        res = CompileShader(&geomShader, GL_GEOMETRY_SHADER, geometryShader.c_str());
        if (!res)
        {
            Log::Error("Shader::Load() Failed to compile geometry shader. File: {}", m_geometryFilename.c_str());
            return false;
        }
    }

    if (!fragmentShader.empty())
    {
        // Create and compile fragment shader
        res = CompileShader(&fragShader, GL_FRAGMENT_SHADER, fragmentShader.c_str());
        if (!res)
        {
            Log::Error("Shader::Load() Failed to compile fragment shader. File: {}", m_fragmentFilename.c_str());
            return false;
        }
    }

    if (!computeShader.empty())
    {
        // Create and compile fragment shader
        res = CompileShader(&compShader, GL_COMPUTE_SHADER, computeShader.c_str());
        if (!res)
        {
            Log::Error("Shader::Load() Failed to compile compute shader. File: {}", m_computeFilename.c_str());
            return false;
        }
    }

    // Attach vertex shader to program
    if (vertShader) glAttachShader(m_program, vertShader);

    // Attach geometry shader to program
    if (geomShader) glAttachShader(m_program, geomShader);

    // Attach fragment shader to program
    if (fragShader) glAttachShader(m_program, fragShader);

    // Attach compute shader to the program
    if (compShader) glAttachShader(m_program, compShader);

    // Link program
    if (!LinkProgram(m_program))
    {
        if (vertShader)
        {
            glDeleteShader(vertShader);
            vertShader = 0;
        }
        if (fragShader)
        {
            glDeleteShader(fragShader);
            fragShader = 0;
        }
        if (geomShader)
        {
            glDeleteShader(geomShader);
            geomShader = 0;
        }
        if (compShader)
        {
            glDeleteShader(compShader);
            compShader = 0;
        }
        if (m_program)
        {
            glDeleteProgram(m_program);
            m_program = 0;
        }

        Log::Error("Shader::Load() Failed to link shader program.");
        return false;
    }

    glDeleteShader(vertShader);
    glDeleteShader(geomShader);
    glDeleteShader(fragShader);
    glDeleteShader(compShader);

    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Compile shader and report success or failure
////////////////////////////////////////////////////////////////////////////////
bool Shader::CompileShader(GLuint* shader, GLenum type, const GLchar* source)
{
    GLint status = 0;

    if (!source)
    {
        Log::Error("Failed to compile empty shader");
        return false;
    }

    *shader = glCreateShader(type);
    glShaderSource(*shader, 1, &source, nullptr);

    glCompileShader(*shader);

#if defined(DEBUG)
    GLint logLength = 0;
    glGetShaderiv(*shader, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 1)
    {
        auto* log = static_cast<GLchar*>(malloc(logLength));
        glGetShaderInfoLog(*shader, logLength, &logLength, log);
        if (log) Log::Error("Program compile log: {}", log);
        free(log);
    }
#endif

    glGetShaderiv(*shader, GL_COMPILE_STATUS, &status);
    if (status == 0)
    {
        glDeleteShader(*shader);
        return false;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Link Shader Program
////////////////////////////////////////////////////////////////////////////////
bool Shader::LinkProgram(GLuint prog)
{
    GLint status = 0;

    glLinkProgram(prog);

#if defined(DEBUG)
    GLint logLength = 0;
    glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 1)
    {
        auto* log = static_cast<GLchar*>(malloc(logLength));
        glGetProgramInfoLog(prog, logLength, &logLength, log);
        if (log) Log::Error("Program link log: {}", log);
        free(log);
    }
#endif

    glGetProgramiv(prog, GL_LINK_STATUS, &status);
    return status != 0;
}

////////////////////////////////////////////////////////////////////////////////
// Validate Shader Program
////////////////////////////////////////////////////////////////////////////////
bool Shader::ValidateProgram(GLuint prog)
{
    GLint logLength = 0;
    GLint status = 0;

    glValidateProgram(prog);
    glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 0)
    {
        auto* log = static_cast<GLchar*>(malloc(logLength));
        glGetProgramInfoLog(prog, logLength, &logLength, log);
        if (log) Log::Error("Program validate log: {}", log);
        free(log);
    }

    logLength = 0;
    glGetProgramiv(prog, GL_VALIDATE_STATUS, &status);
    if (status == 0)
    {
        return false;
    }
    return true;
}

std::string Shader::GetPath(FileIO::Directory, const std::string& vertexFilename, const std::string& fragmentFilename)
{
    return vertexFilename + " " + fragmentFilename;
}

std::string Shader::GetPath(FileIO::Directory,
                            const std::string& vertexFilename,
                            const std::string& geometryFilename,
                            const std::string& fragmentFilename)
{
    return vertexFilename + " " + geometryFilename + " " + fragmentFilename;
}

std::string bee::Shader::GetPath(FileIO::Directory, const std::string& computeFilename) { return computeFilename; }
