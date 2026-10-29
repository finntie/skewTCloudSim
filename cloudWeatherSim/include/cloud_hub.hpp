#pragma once

// Main Class, first access point to library

// Classess
class cloudRenderer;
class simulationHub;
class newInput;
class cloudMenu;
class cameraControl;

struct simParams;

class cloudHub
{
public:

    cloudHub();
    ~cloudHub() = default;

    /// <summary>
    /// Initializes the library, creates all the necessary objects.
    /// Needs openGL context to be created.
    /// Don't forget to call destruct after finishing with the library
    /// </summary>
    void initialize(bool customImGuiStyle);

    /// <summary>
    /// Destructs the library.
    /// </summary>
    void destruct();

    /// <summary>
    /// Updates from the menu, updates most of the files, 
    /// renderer still needs to be handled by developer themselves due to buffer input
    /// </summary>
    /// <param name="onlyView">Only view active, no UI or simulating</param>
    void updateMain(bool onlyView = false);


    void copySimulateDataToRenderData();


    enum resizeFlag
    {
        RESIZE_PARAMS_ONLY_FLAG = 0 << 0,
        RESIZE_SIMULATION_FLAG = 1 << 0,
        RESIZE_RENDERER_FLAG = 1 << 1,
        RESIZE_ALL_FLAG = RESIZE_PARAMS_ONLY_FLAG | RESIZE_SIMULATION_FLAG | RESIZE_RENDERER_FLAG
    };

    // Resizes the whole simulation grid
    // Reallocates data, removing previous saved data
    void resize(int x, int y, int z, float voxelSize, resizeFlag = RESIZE_PARAMS_ONLY_FLAG);

    cloudRenderer& CloudRender() { return *m_cloudRenderObj; }
    simulationHub& CloudSim() { return *m_simulationHubObj; }
    newInput& InputObj() { return *m_inputObj; }
    cameraControl& CameraObj() { return *m_cloudCameraObj; }
    cloudMenu& cloudMenuObj() { return *m_cloudMenuHubObj; }


private:

    cloudMenu* m_cloudMenuHubObj{ nullptr };
    cloudRenderer* m_cloudRenderObj{ nullptr };
    simulationHub* m_simulationHubObj{ nullptr };
    cameraControl* m_cloudCameraObj{ nullptr };
    newInput* m_inputObj{ nullptr };

    // Real instance of the simulation parameters, passed onto all other classes.
    // Determines the size of the simulation / renderer
    simParams* m_params{ nullptr };
};

extern cloudHub CloudHub;