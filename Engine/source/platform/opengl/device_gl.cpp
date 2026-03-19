#include <cassert>
#include "core/device.hpp"
#include "platform/opengl/open_gl.hpp"
#include "tools/log.hpp"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "core/engine.hpp"
#include "rendering/render.hpp"


using namespace bee;

static void ErrorCallback(int, const char* description) { fputs(description, stderr); }
static void ResizeCallBack(GLFWwindow*, int width, int height) 
{
    bee::Engine.Device().setSize(width, height); 
    Engine.ECS().GetSystem<Renderer>().setRenderSize(width, height);
}

void LogOpenGLVersionInfo()
{
    const auto* const vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    const auto* const renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    const auto* const version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    const auto* const shaderVersion = reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));

    Log::Info("OpenGL Vendor {}", vendor);
    Log::Info("OpenGL Renderer {}", renderer);
    Log::Info("OpenGL Version {}", version);
    Log::Info("OpenGL Shader Version {}", shaderVersion);
}

Device::Device()
{
    if (!glfwInit())
    {
        Log::Critical("GLFW init failed");
        assert(false);
        exit(EXIT_FAILURE);
    }

    Log::Info("GLFW version {}.{}.{}", GLFW_VERSION_MAJOR, GLFW_VERSION_MINOR, GLFW_VERSION_REVISION);

    glfwSetErrorCallback(ErrorCallback);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#if defined(DEBUG)
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#else
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_FALSE);
#endif

    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    m_monitor = glfwGetPrimaryMonitor();
    if (m_fullscreen)
    {
        const GLFWvidmode* mode = glfwGetVideoMode(m_monitor);
        m_width = mode->width;
        m_height = mode->height;
        m_window = glfwCreateWindow(m_width, m_height, "BEE", m_monitor, nullptr);
    }
    else
    {
        m_width = 1280;
        m_height = 720;
        m_window = glfwCreateWindow(m_width, m_height, "BEE", nullptr, nullptr);
    }

    if (!m_window)
    {
        Log::Critical("GLFW window could not be created");
        glfwTerminate();
        assert(false);
        exit(EXIT_FAILURE);
    }
    glfwSetWindowSizeCallback(m_window, ResizeCallBack);

    glfwMakeContextCurrent(m_window);

    m_vsync = true;
    if (!m_vsync) glfwSwapInterval(0);

    int major = glfwGetWindowAttrib(m_window, GLFW_CONTEXT_VERSION_MAJOR);
    int minor = glfwGetWindowAttrib(m_window, GLFW_CONTEXT_VERSION_MINOR);
    int revision = glfwGetWindowAttrib(m_window, GLFW_CONTEXT_REVISION);
    Log::Info("GLFW OpenGL context version {}.{}.{}", major, minor, revision);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        Log::Critical("GLAD failed to initialize OpenGL context");
        assert(false);
        exit(EXIT_FAILURE);
    }

    LogOpenGLVersionInfo();
    InitDebugMessages();
}

float bee::Device::GetMonitorUIScale() const
{
    float xscale = 1.0f, yscale = 1.0f;
    glfwGetMonitorContentScale(m_monitor, &xscale, &yscale);
    return xscale;
}

bee::Device::~Device() { glfwTerminate(); }

void bee::Device::Update()
{
    glfwPollEvents();
    glfwSwapBuffers(m_window);
}

void bee::Device::RequestClose() { glfwSetWindowShouldClose(m_window, GL_TRUE); }
bool bee::Device::ShouldClose() { return glfwWindowShouldClose(m_window); }

GLFWwindow* bee::Device::GetWindow() { return m_window; }
