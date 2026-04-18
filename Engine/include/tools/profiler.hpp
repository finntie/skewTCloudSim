#pragma once

#include <unordered_map>
#include <string>
#include <deque>
#include <chrono>
#include <imgui/IconsFontAwesome.h>
#include "inspectable.hpp"
#include "tools/warnings.hpp"

BEE_DISABLE_WARNING_PUSH
BEE_DISABLE_WARNING_UNUSED_PARAMETER
#if defined(BEE_PLATFORM_PC) && defined(BEE_PROFILE)
#include <Superluminal/PerformanceAPI.h>
#else
#define PERFORMANCEAPI_ENABLED 0
#include <Superluminal/PerformanceAPI.h>
#endif
BEE_DISABLE_WARNING_POP

#define BEE_PROFILE_FUNCTION()                  \
    BEE_DISABLE_WARNING_PUSH                    \
    BEE_DISABLE_WARNING_HIDES_LOCAL_DECLARATION \
    bee::ProfilerSection s_sect(__FUNCTION__);  \
    BEE_DISABLE_WARNING_POP                     \
    PERFORMANCEAPI_INSTRUMENT_FUNCTION()

#define BEE_PROFILE_SECTION(id)                 \
    BEE_DISABLE_WARNING_PUSH                    \
    BEE_DISABLE_WARNING_HIDES_LOCAL_DECLARATION \
    bee::ProfilerSection s_sect(id);            \
    BEE_DISABLE_WARNING_POP                     \
    PERFORMANCEAPI_INSTRUMENT(id)

#define BEE_PROFILE_SCOPE(name) bee::ScopeProfiler profiler##__LINE__(name)

#if defined(BEE_PLATFORM_PC)
using clock_type = std::chrono::steady_clock;
#else
using clock_type = std::chrono::system_clock;
#endif

namespace bee
{

using TimeT = std::chrono::time_point<clock_type>;
using SpanT = std::chrono::nanoseconds;

class ProfilerSection
{
public:
    ProfilerSection(std::string name);
    ~ProfilerSection();

private:
    std::string m_name;
};

class ScopeProfiler
{
public:
    ScopeProfiler(const std::string& name);
    ~ScopeProfiler();

protected:
    std::string m_name;
    TimeT m_start;
};

class Profiler : public IPanel
{
public:
    Profiler();        
    void BeginSection(const std::string& name);
    void EndSection(const std::string& name);

//#ifdef BEE_INSPECTOR   
    ~Profiler() override;
    void OnPanel() override;
    std::string GetIcon() const override { return ICON_FA_LINE_CHART; }
    std::string GetName() const override { return "Profiler"; }
//#else
//    ~Profiler();
//#endif

private:
    struct Entry
    {
        TimeT Start{};
        TimeT End{};
        SpanT Accum{};
        float Avg = 0.0f;
        std::deque<float> History;
    };
    std::unordered_map<std::string, Entry> m_times;
};

}  // namespace bee
