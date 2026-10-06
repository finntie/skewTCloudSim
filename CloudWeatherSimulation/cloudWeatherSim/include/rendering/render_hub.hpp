#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

class CudaRender;
class shapesGL;
class cloudFile;

enum class gameStates;


    // Main render class, calls openGL rendering and saves data
class cloudRenderer
{
public:
    cloudRenderer();
    ~cloudRenderer();

    shapesGL& shapesGLObj() { return *m_shapesObj; }
    CudaRender& cudaRendererObj() { return *m_cudaRenderObj; }
    cloudFile& CloudFile() { return *m_cloudFileObj; }

    /// <summary>
    /// Initializes the cloud renderer with the correct buffers
    /// </summary>
    /// <param name="framebuffer"> OpenGL framebuffer, should contain at least a depth buffer. </param>
    /// <param name="colorbuffer"> OpenGL color buffer, (not yet) support for RGB and RGBA</param>
    void init(unsigned int framebuffer, unsigned int colorbuffer);

    /// <summary>
    /// (post) render clouds on top of your geometry by passing your framebuffer containing color and depth buffers.
    /// This function will also draw optional (debug) lines created (internally).
    /// </summary>
    void render(float dt);

    /// <summary>
    /// In case buffers change, use this function to update them correctly
    /// </summary>
    /// <param name="framebuffer"> OpenGL framebuffer, should contain at least a depth buffer. </param>
    /// <param name="colorbuffer"> OpenGL color buffer, (not yet) support for RGB and RGBA</param>
    void updateBuffers(unsigned int framebuffer, unsigned int colorbuffer);

    // Unmaps the resources (depth and color buffer) from CUDA, so that you are allowed to change textures again
    // To map again, call resize()
    void unregisterResources();

    // Call when resizing window
    // Updates all textures on resize of window
    // MAKE SURE TO UNREGISTER RESOURCES BEFORE ACCESSING YOUR COLOR/DEPTH BUFFER ( #unregisterResources() )
    void resize(int width, int height);

    // ImGui panel
    void renderMenuPanel(gameStates& currentState, const bool initState, bool& setInitState);
    void renderPanel(gameStates& currentState, const bool initState, bool& setInitState);
    void renderSettingsPanel();
    void cloudViewMenu();

private:
    void chooseSavedSimulationRun(gameStates& currentState);

    shapesGL* m_shapesObj{nullptr};
    CudaRender* m_cudaRenderObj{nullptr};
    cloudFile* m_cloudFileObj{nullptr};
};