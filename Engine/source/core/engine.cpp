#include "core/engine.hpp"

#include <chrono>
#include <iostream>

#include "core/ecs.hpp"
#include "core/device.hpp"
#include "core/fileio.hpp"
#include "core/input.hpp"
#include "core/audio.hpp"
#include "core/resources.hpp"
#include "rendering/debug_render.hpp"
#include "tools/inspector.hpp"
#include "tools/profiler.hpp"
#include "tools/log.hpp"
#include "tools/thread_pool.hpp"

using namespace bee;

// Make the engine a global variable on free store memory.
EngineClass bee::Engine;

void EngineClass::Initialize()
{
    BEE_PROFILE_SCOPE("Engine Initialize");
    Log::Initialize();
    m_fileIO = new bee::FileIO();
    m_resources = new bee::Resources();
    m_device = new bee::Device();
    m_input = new bee::Input();
    m_audio = new bee::Audio();
    m_debugRenderer = new bee::DebugRenderer();
    m_inspector = new bee::Inspector();
    m_profiler = new bee::Profiler();
    m_ECS = new EntityComponentSystem();
}

void EngineClass::Shutdown()
{
    delete m_ECS;
    delete m_profiler;
    delete m_inspector;
    delete m_debugRenderer;
    delete m_input;
    delete m_audio;
    delete m_device;
    delete m_resources;
    delete m_fileIO;
}

void EngineClass::Run()
{
    auto time = std::chrono::high_resolution_clock::now();
    while (!m_device->ShouldClose())
    {
        auto ctime = std::chrono::high_resolution_clock::now();
        auto elapsed = ctime - time;
        float dt = (float)((double)std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count() / 1000000.0);

        m_input->Update();
        m_audio->Update();
        m_ECS->UpdateSystems(dt);
        m_ECS->RemovedDeleted();
        m_device->BeginFrame();
        m_ECS->RenderSystems();
        m_debugRenderer->Render();
        m_inspector->Inspect(dt);
        m_device->EndFrame();
        m_device->Update();

        time = ctime;
    }
}

ThreadPool& bee::EngineClass::ThreadPool()
{
    if (!m_pool) m_pool = new bee::ThreadPool(4);
    return *m_pool;
}

