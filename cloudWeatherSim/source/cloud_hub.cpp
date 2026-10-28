#include "cloud_hub.hpp"

#include "cloud_menu_hub.hpp"               // (ImGui) Menu
#include "rendering/render_hub.hpp"         // Rendering
#include "simulating/simulation_hub.hpp"    // Simulating
#include "new_input.hpp"                    // Handling Input
#include "camera.hpp"                       // Handling the Camera

#include "simulating/environment.cuh"
#include "rendering/render_hub.hpp"
#include "rendering/cuda_render_gl.hpp"

#include <chrono>
#include <iostream>
#include <mutex>


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

void cloudHub::initialize(bool customImGuiStyle)
{
    m_cloudRenderObj = new cloudRenderer();
    m_simulationHubObj = new simulationHub;
    m_cloudMenuHubObj = new cloudMenu(m_simulationHubObj, m_cloudRenderObj, customImGuiStyle);
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

    // Update render data
    copySimulateDataToRenderData();

    // Render
    m_cloudRenderObj->render(dt);

    // UI (render) is done by user


	time = ctime;
}

void cloudHub::copySimulateDataToRenderData()
{
    if (m_simulationHubObj->getIsSimulationRunning())
    {
        // Get from the simulation thread and copy into the render
        m_simulationHubObj->Environment().synchronizeSimStream(); // TODO: likely have to work with events to fix issues, maybe even let the environment copy data into another buffer that we read
        m_cloudRenderObj->cudaRendererObj().setDataEnvironment(m_simulationHubObj->Environment().getRenderData(), true);
    }
}