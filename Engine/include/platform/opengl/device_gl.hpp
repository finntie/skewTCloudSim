#pragma once

struct GLFWwindow;
struct GLFWmonitor;

namespace bee
{

class Device
{
public:
    bool CanClose() { return true; }
    void RequestClose();
    bool ShouldClose();
    GLFWwindow* GetWindow();
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    void BeginFrame() {}
    void EndFrame() {}
    float GetMonitorUIScale() const;

    //Only called by device_gl, do not call on your own
    void setSize(int width, int height)
    {
        m_width = width;
        m_height = height;
    }

private:
    friend class EngineClass;
    Device();
    ~Device();
    void Update();

    GLFWwindow* m_window = nullptr;
    GLFWmonitor* m_monitor = nullptr;
    bool m_vsync = true;
    bool m_fullscreen = false;
    int m_width = -1;
    int m_height = -1;
};

}  // namespace bee
