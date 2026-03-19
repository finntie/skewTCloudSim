#include "platform/opengl/open_gl.hpp"
#include <string>
#include <cassert>
#include <tools/log.hpp>
#include <GLFW/glfw3.h>

using namespace bee;

#ifdef BEE_DEBUG

static void APIENTRY
DebugCallbackFunc(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei, const GLchar* message, const GLvoid*)
{
    // Skip some less useful info
    if (id == 131218)  // http://stackoverflow.com/questions/12004396/opengl-debug-context-performance-warning
        return;

    // UNUSED(length);
    // UNUSED(userParam);
    std::string sourceString;
    std::string typeString;
    std::string severityString;

    // The AMD variant of this extension provides a less detailed classification of the error,
    // which is why some arguments might be "Unknown".
    switch (source)
    {
        case GL_DEBUG_CATEGORY_API_ERROR_AMD:
        case GL_DEBUG_SOURCE_API:
        {
            sourceString = "API";
            break;
        }
        case GL_DEBUG_CATEGORY_APPLICATION_AMD:
        case GL_DEBUG_SOURCE_APPLICATION:
        {
            sourceString = "Application";
            break;
        }
        case GL_DEBUG_CATEGORY_WINDOW_SYSTEM_AMD:
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
        {
            sourceString = "Window System";
            break;
        }
        case GL_DEBUG_CATEGORY_SHADER_COMPILER_AMD:
        case GL_DEBUG_SOURCE_SHADER_COMPILER:
        {
            sourceString = "Shader Compiler";
            break;
        }
        case GL_DEBUG_SOURCE_THIRD_PARTY:
        {
            sourceString = "Third Party";
            break;
        }
        case GL_DEBUG_CATEGORY_OTHER_AMD:
        case GL_DEBUG_SOURCE_OTHER:
        {
            sourceString = "Other";
            break;
        }
        default:
        {
            sourceString = "Unknown";
            break;
        }
    }

    switch (type)
    {
        case GL_DEBUG_TYPE_ERROR:
        {
            typeString = "Error";
            break;
        }
        case GL_DEBUG_CATEGORY_DEPRECATION_AMD:
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        {
            typeString = "Deprecated Behavior";
            break;
        }
        case GL_DEBUG_CATEGORY_UNDEFINED_BEHAVIOR_AMD:
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
        {
            typeString = "Undefined Behavior";
            break;
        }
        case GL_DEBUG_TYPE_PORTABILITY_ARB:
        {
            typeString = "Portability";
            break;
        }
        case GL_DEBUG_CATEGORY_PERFORMANCE_AMD:
        case GL_DEBUG_TYPE_PERFORMANCE:
        {
            typeString = "Performance";
            break;
        }
        case GL_DEBUG_CATEGORY_OTHER_AMD:
        case GL_DEBUG_TYPE_OTHER:
        {
            typeString = "Other";
            break;
        }
        default:
        {
            typeString = "Unknown";
            break;
        }
    }

    switch (severity)
    {
        case GL_DEBUG_SEVERITY_HIGH:
        {
            severityString = "High";
            break;
        }
        case GL_DEBUG_SEVERITY_MEDIUM:
        {
            severityString = "Medium";
            break;
        }
        case GL_DEBUG_SEVERITY_LOW:
        {
            severityString = "Low";
            break;
        }
        default:
        {
            severityString = "Unknown";
            return;
        }
    }

    Log::Warn("GL Debug Callback:\n source: {}:{} \n type: {}:{} \n id: {} \n severity: {}:{} \n  message: {}",
              source,
              sourceString.c_str(),
              type,
              typeString.c_str(),
              id,
              severity,
              severityString.c_str(),
              message);
    assert(type != GL_DEBUG_TYPE_ERROR);
}

static void APIENTRY
DebugCallbackFuncAMD(GLuint id, GLenum category, GLenum severity, GLsizei length, const GLchar* message, void* userParam)
{
    DebugCallbackFunc(GL_DEBUG_CATEGORY_API_ERROR_AMD, category, id, severity, length, message, userParam);
}

void bee::InitDebugMessages()
{
    // Query the OpenGL function to register your callback function.
    auto _glDebugMessageCallback = (PFNGLDEBUGMESSAGECALLBACKPROC)wglGetProcAddress("glDebugMessageCallback");
    auto _glDebugMessageCallbackARB = (PFNGLDEBUGMESSAGECALLBACKARBPROC)wglGetProcAddress("glDebugMessageCallbackARB");
    auto _glDebugMessageCallbackAMD = (PFNGLDEBUGMESSAGECALLBACKAMDPROC)wglGetProcAddress("glDebugMessageCallbackAMD");

    glDebugMessageCallback(DebugCallbackFunc, nullptr);

    // Register your callback function.
    if (_glDebugMessageCallback != nullptr)
    {
        _glDebugMessageCallback(DebugCallbackFunc, nullptr);
    }
    else if (_glDebugMessageCallbackARB != nullptr)
    {
        _glDebugMessageCallbackARB(DebugCallbackFunc, nullptr);
    }

    // Additional AMD support
    if (_glDebugMessageCallbackAMD != nullptr)
    {
        _glDebugMessageCallbackAMD(DebugCallbackFuncAMD, nullptr);
    }

    // Enable synchronous callback. This ensures that your callback function is called
    // right after an error has occurred. This capability is not defined in the AMD
    // version.
    if ((_glDebugMessageCallback != nullptr) || (_glDebugMessageCallbackARB != nullptr))
    {
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    }

    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_LOW, 0, nullptr, GL_FALSE);

    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
}

#endif

void bee::LabelGL(GLenum type, GLuint name, const std::string& label)
{
    std::string typeString;
    switch (type)
    {
        case GL_BUFFER:
            typeString = "GL_BUFFER";
            break;
        case GL_SHADER:
            typeString = "GL_SHADER";
            break;
        case GL_PROGRAM:
            typeString = "GL_PROGRAM";
            break;
        case GL_VERTEX_ARRAY:
            typeString = "GL_VERTEX_ARRAY";
            break;
        case GL_QUERY:
            typeString = "GL_QUERY";
            break;
        case GL_PROGRAM_PIPELINE:
            typeString = "GL_PROGRAM_PIPELINE";
            break;
        case GL_TRANSFORM_FEEDBACK:
            typeString = "GL_TRANSFORM_FEEDBACK";
            break;
        case GL_SAMPLER:
            typeString = "GL_SAMPLER";
            break;
        case GL_TEXTURE:
            typeString = "GL_TEXTURE";
            break;
        case GL_RENDERBUFFER:
            typeString = "GL_RENDERBUFFER";
            break;
        case GL_FRAMEBUFFER:
            typeString = "GL_FRAMEBUFFER";
            break;
        default:
            typeString = "UNKNOWN";
            break;
    }

    const std::string temp = "[" + typeString + ":" + std::to_string(name) + "] " + label;
    glObjectLabel(type, name, static_cast<GLsizei>(temp.length()), temp.c_str());
}
