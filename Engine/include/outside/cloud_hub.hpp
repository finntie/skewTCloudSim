#pragma once

// Main Class, first access point to library

// Classess
class cloudRenderer;
class simulationHub;
class newInput;
class cloudMenu;
class cameraControl;

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
    void initialize();

    /// <summary>
    /// Destructs the library.
    /// </summary>
    void destruct();

    /// <summary>
    /// Updates from the menu, updates most of the files, 
    /// renderer still needs to be handled by developer themselves due to buffer input
    /// </summary>
    void updateMain();

	cloudRenderer& CloudRender() { return *m_cloudRenderObj; }
    simulationHub& CloudSim() { return *m_simulationHubObj; }
    newInput& InputObj() { return *m_inputObj; }
    cameraControl& CameraObj() { return *m_cloudCameraObj; }
    cloudMenu& cloudMenuObj() { return *m_cloudMenuHubObj; }


private:

    cloudMenu* m_cloudMenuHubObj{nullptr};
	cloudRenderer* m_cloudRenderObj{nullptr};
    simulationHub* m_simulationHubObj{nullptr};
    cameraControl* m_cloudCameraObj{nullptr};
    newInput* m_inputObj{nullptr};
};

extern cloudHub CloudHub;

void lockGlobal();
void unlockGlobal();