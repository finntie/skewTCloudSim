#include "tools/profiler.hpp"

#if defined(BEE_PROFILE)
#if defined(BEE_PLATFORM_PC)
#include <Windows.h>
#include <Superluminal/PerformanceAPI_loader.h>
#endif
#include "tools/log.hpp"
#include "core/engine.hpp"
#include <imgui/imgui.h>
#include <imgui/implot.h>

using namespace bee;

ProfilerSection::ProfilerSection(std::string name) : m_name(std::move(name))
{ Engine.Profiler().BeginSection(m_name); }

ProfilerSection::~ProfilerSection() { Engine.Profiler().EndSection(m_name); }

ScopeProfiler::ScopeProfiler(const std::string& name) : m_name(name), m_start(std::chrono::high_resolution_clock::now()) {}

ScopeProfiler::~ScopeProfiler()
{
    Log::Info("{}: {}",
              m_name.c_str(),
              std::chrono::duration<float, std::milli>(std::chrono::high_resolution_clock::now() - m_start).count());
}

Profiler::Profiler()
{
#if defined(BEE_PLATFORM_PC)
    PerformanceAPI_Functions performanceAPI;
    if (!PerformanceAPI_LoadFrom(L"PerformanceAPI.dll", &performanceAPI))
        Log::Warn("Superluminal PerformanceAPI could not be loaded");
    PerformanceAPI_SetCurrentThreadName("Main");
#endif  // BEE_PLATFORM_PC
}

Profiler::~Profiler() {}

void Profiler::BeginSection(const std::string& name)
{ m_times[name].Start = std::chrono::high_resolution_clock::now(); }

void Profiler::EndSection(const std::string& name)
{
    auto& e = m_times[name];
    e.End = std::chrono::high_resolution_clock::now();
    auto elapsed = e.End - e.Start;
    e.Accum += elapsed;
}

//#if defined(BEE_INSPECTOR)

void Profiler::OnPanel()
{
    for (auto& itr : m_times)
    {
        auto& e = itr.second;
        auto duration = (float)((double)e.Accum.count() / 1e+6);
        if (e.History.size() > 100) e.History.pop_front();
        e.History.push_back(duration);

        e.Avg = 0.0f;
        for (float f : e.History) e.Avg += f;

        e.Avg /= (float)e.History.size();
    }

    if (ImPlot::BeginPlot("Profiler"))
    {
        ImPlot::SetupAxes("Sample", "Time");
        ImPlot::SetupAxesLimits(0, 100, 0, 20);
        for (auto& itr : m_times)
        {
            auto& e = itr.second;

            std::vector<float> vals(e.History.begin(), e.History.end());

            ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, 0.25f);
            ImPlot::PlotShaded(itr.first.c_str(), vals.data(), (int)vals.size());
            ImPlot::PopStyleVar();
            ImPlot::PlotLine(itr.first.c_str(), vals.data(), (int)vals.size());
        }
        ImPlot::EndPlot();
    }

    for (auto& itr : m_times) ImGui::Text("%s: %f ms", itr.first.c_str(), itr.second.Avg);
    for (auto& itr : m_times) itr.second.Accum = {};
}
//#endif

#else

bee::ProfilerSection::ProfilerSection(const std::string& name) {}
bee::ProfilerSection::~ProfilerSection() {}

bee::Profiler::Profiler() {}
bee::Profiler::~Profiler() {}
void bee::Profiler::Inspect() {}

#endif


