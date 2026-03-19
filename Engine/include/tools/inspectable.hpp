#pragma once
#include <vector>
#include <string>
#include "core/ecs.hpp"

//#ifdef BEE_INSPECTOR

namespace bee
{

template <typename T>
class EditorBase
{
public:
    virtual ~EditorBase() { m_editors.erase(std::find(m_editors.begin(), m_editors.end(), this)); }
protected:
    static std::vector<T*> m_editors;
    friend class Inspector;
};

/// <summary>
/// Inherit from this if you want your class to have ImGui behavior in the main Bee toolbar.
/// </summary>
class IToolbar : public EditorBase<IToolbar>
{
public:
    IToolbar() { m_editors.push_back(this); }
    virtual void OnToolbar() = 0;
};

/// <summary>
/// Inherit from this if you want your class to display (e.g. component) data of the selected Bee ECS entity in the Scene panel.
/// </summary>
class IEntityInspector : public EditorBase<IEntityInspector>
{
public:
    IEntityInspector() { m_editors.push_back(this); }
    virtual void OnEntity(entt::entity entity) = 0;
};

/// <summary>
/// Inherit from this if you want your class to have a full panel in the Bee inspector, next to the Scene panel.
/// For example, the Bee Profiler uses this to display profiling output.
/// </summary>
class IPanel : public EditorBase<IPanel>
{
public:
    IPanel() { m_editors.push_back(this); }
    virtual void OnPanel() = 0;
    virtual std::string GetName() const = 0;
    virtual std::string GetIcon() const = 0;
};

/// <summary>
/// Inherit from this if you want your class to display data in the main Bee stats bar.
/// </summary>
class IStatsBar : public EditorBase<IStatsBar>
{
public:
    IStatsBar() { m_editors.push_back(this); }
    virtual void OnStatsBar() = 0;
};

}

//#else

//namespace bee
//{
//
//class IToolbar {};
//class IEntityInspector {};
//class IPanel {};
//class IStatsBar {};
//
//}

//#endif

