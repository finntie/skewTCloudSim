#pragma once

#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

class CudaRender;
class shapesGL;
class cloudFile;
class cloudFileHub;
struct simParams;

enum class gameStates;


    // Main render class, calls openGL rendering and saves data
class cloudRenderer
{
public:
    cloudRenderer(const simParams& params);
    ~cloudRenderer();

    shapesGL& shapesGLObj() { return *m_shapesObj; }
    CudaRender& cudaRendererObj() { return *m_cudaRenderObj; }
    cloudFile& CloudFile() { return *m_cloudFileObj; }
    cloudFileHub& CloudFileHub() { return *m_cloudFileHubObj; }

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
    /// Resize GPU data with current simulation parameters.
    /// </summary>
    void resizeGPU();

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
    void resizeWindow(int width, int height);

    // ImGui panel
    void renderMenuPanel(gameStates& currentState, const bool initState, bool& setInitState);
    void renderPanel(gameStates& currentState, const bool initState, bool& setInitState);
    void renderSettingsPanel();
    void cloudViewMenu();

    /// <summary>
    /// Load cloud file data and possibly also the render settings
    /// </summary>
    /// <param name="cloudFile">File containing cloud/precip data</param>
    void loadFile(const char* cloudFile);

    /// <summary>
    /// Load render settings file, sets the current render settings to be the values from within the file
    /// </summary>
    /// <param name="renderSettingsFile">File containing render settings, can be nullptr and default settings will be used.</param>
    void loadRenderSettingsFile(const char* renderSettingsFile);

private:
    void chooseSavedSimulationRun(gameStates& currentState);


    shapesGL* m_shapesObj{nullptr};
    CudaRender* m_cudaRenderObj{nullptr};
    cloudFile* m_cloudFileObj{nullptr};
    cloudFileHub* m_cloudFileHubObj{nullptr};
    const simParams& m_params;

};