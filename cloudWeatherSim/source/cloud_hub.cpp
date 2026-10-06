#include "cloud_hub.hpp"

#include "cloud_menu_hub.hpp"               // (ImGui) Menu
#include "rendering/render_hub.hpp"         // Rendering
#include "simulating/simulation_hub.hpp"    // Simulating
#include "new_input.hpp"                    // Handling Input
#include "camera.hpp"                       // Handling the Camera

#include <chrono>
#include <iostream>
#include <mutex>

// Global mutex to lock parts of code for threading
// Recursive mutex for possible multiple locks
std::recursive_mutex mtx;

// Global variable for the cloud simulation
cloudHub CloudHub;

// Minimum of 2.5 fps
float maxDt = 1 / 2.5f;

cloudHub::cloudHub() 
{
    // We may already create input context
    m_inputObj = new newInput;
    m_cloudCameraObj = new cameraControl();
}

void cloudHub::initialize()
{
    m_cloudRenderObj = new cloudRenderer();
    m_simulationHubObj = new simulationHub;
    m_cloudMenuHubObj = new cloudMenu(m_simulationHubObj, m_cloudRenderObj);
}

void cloudHub::destruct()
{
    delete m_cloudMenuHubObj;
    delete m_cloudRenderObj;
    delete m_simulationHubObj;
    delete m_cloudCameraObj;
    delete m_inputObj;
}

void cloudHub::updateMain(bool onlyView)
{
	static auto time = std::chrono::high_resolution_clock::now();

    auto ctime = std::chrono::high_resolution_clock::now();
    auto elapsed = ctime - time;
    float dt = (float)((double)std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count() / 1000000.0);
    dt = std::min(dt, maxDt);

    // First handle input for camera
    m_inputObj->updateInput();
    m_cloudCameraObj->update(dt);

    // Simulate
    if (!onlyView) m_simulationHubObj->updateSimulation(dt);

    // Render
    m_cloudRenderObj->render(dt);

    // UI (render)
    if (!onlyView) m_cloudMenuHubObj->update();


	time = ctime;
}



void lockGlobal() { mtx.lock(); }

void unlockGlobal() { mtx.unlock(); }
