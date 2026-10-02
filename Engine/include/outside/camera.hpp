#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct GLFWwindow;

struct cameraData
{
    // Values are pointers to create the ability to change them
    // TODO: add option for developer to gain ability to change camera or not by not allowing change
    glm::vec3 m_cameraPos{};
    glm::quat m_cameraRotation{};

    glm::mat4 m_view{};
    glm::mat4 m_projection{};

    float m_cameraNear = 0.0f;
    float m_cameraFar = 0.0f;
};

class cameraControl
{
public:

    cameraControl();
    ~cameraControl();

    /// <summary>
    /// Sets camera variables and callbacks for glfw.
    /// Make sure that own glfw callbacks are already initialized before calling this to prevent overwritten this library's callbacks!
    /// </summary>
    /// <param name="GLFWwindow"> GLFW window context, used to handle window resizing and input internally </param> 
    /// <param name="pos">Camera position</param> 
    /// <param name="rotation">Camera rotation</param> 
    /// <param name="projection">Projection matrix from the camera</param>
    /// <param name="userCamera">May the library handle input themselves? Creates for mismatch in geometry and clouds if user also handles camera, if true, you may pass camera data each frame.</param>
    void initialize(GLFWwindow* window, glm::vec3 pos, glm::quat rotation, glm::mat4 projection, bool userCamera = false);

    /// <summary>
    /// Update the camera with user camera data, make sure userCamera was set to true in #initialize()
    /// </summary>
    /// <param name="pos">Camera position</param>
    /// <param name="rotation">Camera rotation</param> 
    void updateCamera(glm::vec3 pos, glm::quat rotation);

    void update(float dt);

    void enable() { m_active = true; }
    void disable() { m_active = false; }

    void setCamPos(glm::vec3 pos);
    void setCamRot(glm::quat rotation);

    glm::vec3 getCamPos(); 
    glm::quat getCamRot(); 
    float getCamNear();
    float getCamFar();

    glm::mat4& getCamProjection();

    glm::mat4& getCamView();


    int getScrWidth();
    int getScrHeight();


    glm::vec3 getMousePos3D() { return MousePos3D; }

    GLFWwindow* getWindow() { return m_window; }

private:

    void controllingTheCamera(float dt);

    // Check if window size is still correct,
    // That is, if glfw window size was different for more than 1 frame, it means our callback did not fire
    // Likely meaning our callback got overwritten
    void checkWindowUpdate();

    cameraData* m_cameraObj{nullptr};

    bool m_active{true};
    bool m_userCamera{false};

    // Device variables
    GLFWwindow* m_window{nullptr};

    // Control variables
    glm::vec3 MousePos3D{};
    glm::vec3 SaveMousePos{0.0f};
    glm::vec2 Save2DPos{0.0f};
    float roll = 0.0f, pitch = 0.0f;
    float SaveRoll = 45.0f, SavePitch = -45.0f;
    float MouseWheel = 0;
};