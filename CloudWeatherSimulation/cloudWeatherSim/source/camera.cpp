#include "camera.hpp"

#include "cloud_hub.hpp"
#include "new_input.hpp"
#include "rendering/render_hub.hpp"

#include "math/geometry.hpp"

#include <GLFW/glfw3.h>
#include <iostream>


cameraControl::cameraControl() 
{
    m_cameraObj = new cameraData();
}

cameraControl::~cameraControl() 
{
    delete m_cameraObj; 
}

GLFWwindowsizefun windowSizeOldCallback = nullptr;

int m_width{0};
int m_height{0};

static void windowResizeCallBack(GLFWwindow* window, int width, int height)
{
    if (height < 10) return; // Don't update resizing if this small

    // Unregister cuda with openGL, so the user may update their openGL
    CloudHub.CloudRender().unregisterResources();

    // call previous set resize callback, likely updating user's openGL
    if (windowSizeOldCallback) windowSizeOldCallback(window, width, height);

    // Update own openGL stuff and register again
    CloudHub.CloudRender().resize(width, height);

    m_width = width;
    m_height = height;
}

void cameraControl::initialize(GLFWwindow* window, glm::vec3 pos, glm::quat rotation, glm::mat4 projection, bool userCam)
{
    m_cameraObj->m_cameraPos = pos;
    m_cameraObj->m_cameraRotation = rotation;

    std::cout << "Window pointer (exe side): " << window << std::endl;

    // Retrieve near and far from camera
    float A = projection[2][2];
    float B = projection[3][2];
    m_cameraObj->m_cameraNear = B / (A - 1.0f);
    m_cameraObj->m_cameraFar = B / (A + 1.0f);

    // Construct view matrix
    glm::vec3 forward = rotation * glm::vec3(0, 0, -1);
    glm::mat4 view = glm::lookAt(pos, pos + forward, glm::vec3(0, 1, 0));

    m_cameraObj->m_view = view;
    m_cameraObj->m_projection = projection;

    m_userCamera = userCam;

    // Device
    m_window = window;

    glfwGetWindowSize(m_window, &m_width, &m_height);

    printf("Window size; %i, %i\n", m_width, m_height);

    // Set resize callback
    windowSizeOldCallback = glfwSetWindowSizeCallback(m_window, windowResizeCallBack);

    // With the window set up, we can initialize.
    CloudHub.InputObj().initialize();
}



void cameraControl::updateCamera(glm::vec3 pos, glm::quat rotation) 
{
    if (!m_userCamera)
    {
        printf("CLOUDLIB WARNING: camera is set to not be updated by the user, yet updateCamera is called, note cameraObj().initialize()");
        return;
    }
    m_cameraObj->m_cameraPos = pos;
    m_cameraObj->m_cameraRotation = rotation;
    glm::vec3 forward = rotation * glm::vec3(0, 0, -1);
    glm::mat4 view = glm::lookAt(pos, pos + forward, glm::vec3(0, 1, 0));

    m_cameraObj->m_view = view;
}

void cameraControl::update(float dt)
{
    MousePos3D = screenToGround(CloudHub.InputObj().getMousePos(), getCamPos(), getCamRot(), getCamProjection());

    // Error check
    checkWindowUpdate();

    // Only handle camera control if user is not doing it
    if (!m_userCamera) controllingTheCamera(dt);

    // Set camera view
    glm::vec3 forward = getCamRot() * glm::vec3(0, 0, -1);
    m_cameraObj->m_view = glm::lookAt(getCamPos(), getCamPos() + forward, glm::vec3(0, 1, 0));

    // Reset values
    MouseWheel = CloudHub.InputObj().mouseScroll();
}

void cameraControl::setCamPos(glm::vec3 pos) { m_cameraObj->m_cameraPos = pos; }
void cameraControl::setCamRot(glm::quat rotation) { m_cameraObj->m_cameraRotation = rotation; }

glm::vec3 cameraControl::getCamPos() { return m_cameraObj->m_cameraPos; }
glm::quat cameraControl::getCamRot() { return m_cameraObj->m_cameraRotation; }

float cameraControl::getCamNear() { return m_cameraObj->m_cameraNear; }
float cameraControl::getCamFar() { return m_cameraObj->m_cameraFar; }

glm::mat4& cameraControl::getCamProjection() { return m_cameraObj->m_projection; }
glm::mat4& cameraControl::getCamView() { return m_cameraObj->m_view; }

int cameraControl::getScrWidth() 
{
    return m_width;
}

int cameraControl::getScrHeight() 
{
    return m_height;
}

void cameraControl::controllingTheCamera(float dt) 
{
        // Reset state until we de-selected the inspector
    if (!m_active)
    {
        SaveMousePos = MousePos3D;
        Save2DPos = CloudHub.InputObj().getMousePos();
        MousePos3D = screenToGround(Save2DPos, getCamPos(), getCamRot(), getCamProjection());
        return;
    }

    // Keybinds = Left-Shift + mouse
    bool LeftShift = CloudHub.InputObj().keyDown(newInput::LEFT_SHIFT);
    bool LeftCrtl = CloudHub.InputObj().keyDown(newInput::LEFT_CONTROL);

    // Only able to move when active
    if (m_active)
    {
        // For each camera (we have 1)

        //------------------------------------------------------------------------------
        //--------------------------Moving around---------------------------------------
        //------------------------------------------------------------------------------

        float cameraSpeed = 25.0f;
        if (LeftShift)
        {
            cameraSpeed *= 10.0f;
        }

        glm::vec3 forward = getCamRot() * glm::vec3(0, 0, -1) * cameraSpeed;
        glm::vec3 right = getCamRot() * glm::vec3(1, 0, 0) * cameraSpeed;
        glm::vec3 up = getCamRot() * glm::vec3(0, 1, 0) * cameraSpeed;

        // Using keys
        if (CloudHub.InputObj().keyDown(newInput::W))
        {
            setCamPos(getCamPos() + forward * dt);
        }
        if (CloudHub.InputObj().keyDown(newInput::A))
        {
            setCamPos(getCamPos() - right * dt);
        }
        if (CloudHub.InputObj().keyDown(newInput::S))
        {
            setCamPos(getCamPos() - forward * dt);
        }
        if (CloudHub.InputObj().keyDown(newInput::D))
        {
            setCamPos(getCamPos() + right * dt);
        }
        if (CloudHub.InputObj().keyDown(newInput::Q))
        {
            setCamPos(getCamPos() - up * dt);
        }
        if (CloudHub.InputObj().keyDown(newInput::E))
        {
            setCamPos(getCamPos() + up * dt);
        }

        // Using mouse
        if (CloudHub.InputObj().mouseOnce(newInput::MOUSE_LEFT))
        {
            SaveMousePos = MousePos3D;
        }
        else if (CloudHub.InputObj().mouseDown(newInput::MOUSE_LEFT))
        {
            glm::vec3 offset = (SaveMousePos - MousePos3D);
            const float limit = 500;  // Set limit to not move infinite amount
            if (abs(offset.x) + abs(offset.y) + abs(offset.z) > limit) offset = glm::normalize(offset) * limit;

            setCamPos(getCamPos() + offset);
        }

        //------------------------------------------------------------------------------
        //--------------------------Looking around--------------------------------------
        //------------------------------------------------------------------------------
        if (CloudHub.InputObj().mouseOnce(newInput::MOUSE_RIGHT))
        {
            Save2DPos = CloudHub.InputObj().getMousePos();
            // Save previous roll and pitch outside of loop
            roll = SaveRoll;
            pitch = SavePitch;
        }
        else if (CloudHub.InputObj().mouseDown(newInput::MOUSE_RIGHT))
        {
            // With help from chatGPT

            //---------------------------Set Roll and Pitch for Camera Rotation------------------------
            glm::vec2 mousePos2D = CloudHub.InputObj().getMousePos();
            SaveRoll = mousePos2D.x - Save2DPos.x;
            SavePitch = mousePos2D.y - Save2DPos.y;

            // Apply sensitivity
            SaveRoll *= 0.075f;
            SavePitch *= 0.075f;

            // Apply previous roll and pitch
            SaveRoll = SaveRoll + roll;
            SavePitch = SavePitch + pitch;

            if (SavePitch > 89.9f) SavePitch = 89.9f;
            if (SavePitch < -89.9f) SavePitch = -89.9f;

            // Quats for roll and pitch
            glm::quat qRoll = glm::angleAxis(glm::radians(SaveRoll), glm::vec3(0, 1, 0));
            glm::quat qPitch = glm::angleAxis(glm::radians(SavePitch), glm::vec3(1, 0, 0));
            // Combine them
            glm::quat InputRotation = qRoll * qPitch;

            // Set camera rotation
            setCamRot(InputRotation);
        }

        //------------------------------------------------------------------------------
        //--------------------------Zooming in------------------------------------------
        //------------------------------------------------------------------------------

        // if shift is not pressed
        if (!LeftCrtl && MouseWheel != CloudHub.InputObj().mouseScroll())  // Mouse has been scrolled
        {
            float difference = CloudHub.InputObj().mouseScroll() - MouseWheel;
            glm::vec3 dir = MousePos3D - getCamPos();
            dir *= 0.2f;  // We do not want to zoom on top of the position, just towards it.
            // Casual P = O + D*T
            setCamPos(getCamPos() + dir * difference);
        }
    }
}

void cameraControl::checkWindowUpdate()
{
    static bool previousSuccess = true;

    int width = 0, height = 0;
    glfwGetWindowSize(m_window, &width, &height);

    if (width == m_width && height == m_height) previousSuccess = true; 
    else if (previousSuccess) previousSuccess = false;
    else
    {
        // If previous frame it also failed, it is likely we skipped a resize. 
        // This could be a problem, since it seems like our callback has been overwritten
        printf("CLOUDLIB WARNING: Window size was not correctly updated, "
            "was cameraObj().initialize() called before own resize callback, "
            "which may has overwritten this library's callback?");
    }
}
