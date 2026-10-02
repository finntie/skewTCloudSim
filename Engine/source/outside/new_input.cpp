#include "outside/new_input.hpp"

#include "outside/cloud_hub.hpp"
#include "outside/camera.hpp"
#include "outside/cloud_menu_hub.hpp"

#include <GLFW/glfw3.h>

// Due to callbacks possibly already be taken, we save old function, and call them ourself
// This way, we do not consume the callback and can still listen and act upon the callback ourself
GLFWcursorposfun cursorOldCallback = nullptr;
GLFWkeyfun keyOldCallback = nullptr;
GLFWmousebuttonfun mousebuttonOldCallback = nullptr;
GLFWscrollfun scrollOldCallback = nullptr;

glm::vec2 m_mousepos;
float m_mousewheel = 0;
bool keysDown[MAXKEYS]{false};
bool mouseButDown[MAXBUTTONS]{false};

enum KeyAction
{
    Release = 0,
    Press = 1,
    None = 2
};

void cursorPosCallback(GLFWwindow* win, double xpos, double ypos)
{
    m_mousepos.x = (float)xpos;
    m_mousepos.y = (float)ypos;

    // ImGui Callback
    CloudHub.cloudMenuObj().callbackImGuiCursorPos(win, xpos, ypos);

    if (cursorOldCallback) cursorOldCallback(win, xpos, ypos);
}

void scrollCallback(GLFWwindow* win, double xoffset, double yoffset) 
{ 
    m_mousewheel += (float)yoffset; 

    // ImGui Callback
    CloudHub.cloudMenuObj().callbackImGuiScroll(win, xoffset, yoffset);

    if (keyOldCallback) scrollOldCallback(win, xoffset, yoffset);
}

void keyCallback(GLFWwindow* win, int key, int scan, int action, int mods)
{
    if (action == GLFW_PRESS || action == GLFW_RELEASE) keysDown[key] = static_cast<KeyAction>(action);

    // ImGui Callback
    CloudHub.cloudMenuObj().callbackImGuiKey(win, key, scan, action, mods);

    if (keyOldCallback) keyOldCallback(win, key, scan, action, mods);
}

void mousebuttonCallback(GLFWwindow* win, int button, int action, int mods)
{
    if (action == GLFW_PRESS || action == GLFW_RELEASE) mouseButDown[button] = static_cast<KeyAction>(action);

    // ImGui Callback
    CloudHub.cloudMenuObj().callbackImGuiMouseButton(win, button, action, mods);

    if (keyOldCallback) mousebuttonOldCallback(win, button, action, mods);
}


void newInput::initialize() 
{
    auto window = CloudHub.CameraObj().getWindow();

    // Set callbacks and possible previous callbacks.
    cursorOldCallback = glfwSetCursorPosCallback(window, cursorPosCallback);
    keyOldCallback = glfwSetKeyCallback(window, keyCallback);
    mousebuttonOldCallback = glfwSetMouseButtonCallback(window, mousebuttonCallback);
    scrollOldCallback = glfwSetScrollCallback(window, scrollCallback);


    glfwGetWindowSize(window, &m_initialScrWidth, &m_initialScrHeight);
    m_scrWidth = m_initialScrHeight;
    m_scrHeight = m_initialScrHeight;

    // First updateInput
    updateInput();
}

void newInput::updateInput()
{
    // Update window size
    glfwGetWindowSize(CloudHub.CameraObj().getWindow(), &m_scrWidth, &m_scrHeight);

    // First Key updateInput
    for (int i = 0; i < MAXKEYS; i++)
    {
        if (keysDown[i])
        {
            // Hold is true when keysDown was at least for 2 frames down
            if (pressedKeysOnce[i])
            {
                pressedKeysHold[i] = true;
                pressedKeysOnce[i] = false;
            }
            else if (!pressedKeysHold[i])
            {
                // Only once should this be true when holding
                pressedKeysOnce[i] = true;
            }
        }
        else
        {
            pressedKeysOnce[i] = false;
            pressedKeysHold[i] = false;
        }
    }
    // Now mouse updateInput
    for (int i = 0; i < MAXBUTTONS; i++)
    {
        if (mouseButDown[i])
        {
            // Hold is true when keysDown was at least for 2 frames down
            if (mouseButOnce[i])
            {
                mouseButHold[i] = true;
                mouseButOnce[i] = false;
            }
            else if (!mouseButHold[i])
            {
                // Only once should this be true when holding
                mouseButOnce[i] = true;
            }
        }
        else
        {
            mouseButOnce[i] = false;
            mouseButHold[i] = false;
        }
    }
}

float newInput::mouseScroll() 
{
    return m_mousewheel; }

glm::vec2 newInput::getMousePos() 
{
    return m_mousepos; 
}

glm::vec2 newInput::getRelativeMousePos() 
{
    return m_mousepos * glm::vec2(float(m_initialScrWidth) / float(m_scrWidth), float(m_initialScrHeight) / float(m_scrHeight));
}