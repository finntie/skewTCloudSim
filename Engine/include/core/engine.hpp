#pragma once
#include <string>

#define BEE_VERSION "2425.C.1"

namespace bee
{

class EntityComponentSystem;
class FileIO;
class Resources;
class Device;
class Input;
class Audio;
class DebugRenderer;
class Inspector;
class Profiler;
class ThreadPool;

class EngineClass
{
public:
    void Initialize();
    void Shutdown();
    void Run();

    FileIO& FileIO() { return *m_fileIO; }
    Resources& Resources() { return *m_resources; }
    Device& Device() { return *m_device; }
    Input& Input() { return *m_input; }
    Audio& Audio() { return *m_audio; }
    DebugRenderer& DebugRenderer() { return *m_debugRenderer; }
    Inspector& Inspector() { return *m_inspector; }
    Profiler& Profiler() { return *m_profiler; }
    EntityComponentSystem& ECS() { return *m_ECS; }
    ThreadPool& ThreadPool();  // Thread pool does lazy initialization
    inline const std::string& GetVersionString() { return m_versionString; }

private:
    bee::FileIO* m_fileIO = nullptr;
    bee::Resources* m_resources = nullptr;
    bee::Device* m_device = nullptr;
    bee::DebugRenderer* m_debugRenderer = nullptr;
    bee::Input* m_input = nullptr;
    bee::Audio* m_audio = nullptr;
    bee::Inspector* m_inspector = nullptr;
    bee::Profiler* m_profiler = nullptr;
    bee::ThreadPool* m_pool = nullptr;
    EntityComponentSystem* m_ECS = nullptr;

    std::string m_versionString = BEE_VERSION;
};

extern EngineClass Engine;

}  // namespace bee