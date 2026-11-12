#include "cloud_hub.hpp"

#include "cloud_menu_hub.hpp"               // (ImGui) Menu
#include "rendering/render_hub.hpp"         // Rendering
#include "simulating/simulation_hub.hpp"    // Simulating
#include "new_input.hpp"                    // Handling Input
#include "camera.hpp"                       // Handling the Camera

#include "utils.cuh"

#include "simulating/environment.cuh"
#include "rendering/render_hub.hpp"
#include "rendering/cuda_render_gl.hpp"
#include "rendering/cloudFile_hub.hpp"

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
    m_params = new simParams(simParams::create(128, 38, 128, 300.0f));
}

void cloudHub::initialize(bool customImGuiStyle)
{
    m_cloudRenderObj = new cloudRenderer(*m_params);
    m_simulationHubObj = new simulationHub(*m_params);
    m_cloudMenuHubObj = new cloudMenu(m_simulationHubObj, m_cloudRenderObj, customImGuiStyle);
}

void cloudHub::destruct()
{
    delete m_cloudMenuHubObj;
    delete m_cloudRenderObj;
    delete m_simulationHubObj;
    delete m_params;
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
        // Get events to mark if we are allowed to touch the data
        void* eventSim;
        void* eventRender;
        m_simulationHubObj->Environment().getEvents(eventSim, eventRender);
        // Get from the simulation thread and copy into the render
        m_cloudRenderObj->cudaRendererObj().setDataEnvironmentEvent(m_simulationHubObj->Environment().getSimulationData(), true, eventSim, eventRender);
    }
}

void cloudHub::resize(int x, int y, int z, float voxelSize, resizeFlag flag)
{
    *m_params = simParams::create(x, y, z, voxelSize);

    // TODO: re allocate all data in simulation

    if (flag & RESIZE_RENDERER_FLAG)
    {
        m_cloudRenderObj->resizeGPU();
    }
    if (flag & RESIZE_SIMULATION_FLAG)
    {
        m_simulationHubObj->resizeGPU();
    }
}
