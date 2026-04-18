#pragma once

#include <map>
#include <set>
#include <string>
#include <glm/glm.hpp>
#include <imgui/imgui.h>
#include <imgui/ImGuizmo.h>
#include <imgui/IconsFontAwesome.h>

#include "inspectable.hpp"
#include "core/ECS.hpp"
#include "core/ecs.hpp"
#include "core/engine.hpp"
#include "core/transform.hpp"
#include "core/geometry2d.hpp"
#include "tools/serialization.hpp"

namespace bee
{

//#ifdef BEE_INSPECTOR

class SceneInspector : public IPanel, public IEntityInspector
{
public:
    SceneInspector();
    ~SceneInspector() override = default;
    void OnPanel() override;
    std::string GetName() const override { return "Scene"; }
    std::string GetIcon() const override { return ICON_FA_SITEMAP; }
    void OnEntity(entt::entity entity) override;
    static bool ComponentHeader(const char* label, ImGuiTreeNodeFlags flags);
    Entity SelectedEntity() { return m_selectedEntity; }
    void ManipToolbar(ImVec2 pos);
    void Gizmo(const glm::mat4& view, const glm::mat4& projection);

protected:    
    void Inspect(Entity entity, Transform& transform, std::set<Entity>& inspected);
    static bool DragFloat3(const char* label,
                           glm::vec3& v,
                           float speed = 1.0f,
                           float min = 0.0f,
                           float max = 0.0f,
                           const char* format = "%.3f",
                           ImGuiSliderFlags flags = 0);
    void Filter(
        entt::basic_view<entt::get_t<entt::sigh_mixin<entt::basic_storage<Transform>>>, entt::exclude_t<>>& entities,
        std::set<entt::entity>& inspected_or_filtered,
        const ImGuiTextFilter& filter);

    entt::entity m_selectedEntity = entt::null;
    ImGuizmo::OPERATION m_gizmoOperation = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE m_gizmoMode = ImGuizmo::WORLD;
};
//#else
//class SceneInspector {};
//#endif

class Inspector
{
public:
    Inspector();
    ~Inspector();
    void SaveToFile();
    void InitFromFile();
    bool IsMouseOver();
    bool IsSelected();
    void SetVisible(bool visible) { m_visible = visible; }
    bool GetVisible() const { return m_visible; }
    void Inspect(float dt);
    void Inspect(Entity entity, Transform& transform, std::set<Entity>& inspected);
    static bool Inspect(const char* name, float& f);
    static bool Inspect(const char* name, int& i);
    static bool Inspect(const char* name, bool& b);
    static bool Inspect(const char* name, glm::vec2& v);
    static bool Inspect(const char* name, glm::vec3& v);
    static bool Inspect(const char* name, glm::vec4& v);

    template <typename T, std::enable_if_t<std::is_enum<T>::value, bool> = true>
    static void Inspect(const char* name, T& en)
    {
        const auto& allNames = magic_enum::enum_names<T>();
        const auto currentIndex = magic_enum::enum_index<T>(en);
        const std::string currentName(magic_enum::enum_name(en));

        if (ImGui::BeginCombo(name, currentName.c_str()))
        {
            for (size_t i = 0; i < allNames.size(); ++i)
            {
                std::string valueName(allNames[i]);
                if (ImGui::Selectable(valueName.c_str(), i == currentIndex))
                {
                    en = magic_enum::enum_value<T>(i);
                }
            }
            ImGui::EndCombo();
        }
    }

    template <typename T>
    static void Inspect(const char* name, std::vector<T>& v)
    {
        ImGui::Text(name);
        ImGui::Indent(10.f);

        size_t pressedIndex = std::numeric_limits<size_t>::max();
        for (size_t i = 0; i < v.size(); ++i)
        {
            ImGui::PushID((int)i);
            if (ImGui::Button("-", ImVec2(26, 26))) pressedIndex = i;
            ImGui::PopID();

            ImGui::SameLine();
            std::string subname(std::string("Element ") + std::to_string(i));
            Inspect(subname.c_str(), v[i]);
        }

        if (pressedIndex != std::numeric_limits<size_t>::max()) v.erase(v.begin() + pressedIndex);

        if (ImGui::Button("+", ImVec2(26, 26))) v.push_back(T());

        ImGui::Unindent(10.f);
    }

    template <typename T>
    inline void operator()(const char* name, T& value)
    {
        Inspect(name, value);
    }
    template <typename T, std::enable_if_t<visit_struct::traits::is_visitable<T>::value, bool> = true>
    inline void Inspect(T& value)
    {
        visit_struct::for_each(value, Engine.Inspector());
    }

    static void Tooltip(const char* text);

    bool m_visible = true;
    bool m_selectedTab = false;

private:

    void Toolbar();
    void Panel();
    void Stats();
    void Gizmo();
    void Selected();


    SceneInspector m_scene;

    int m_selectedPanel = 0;
    
    bool m_showImguiTest = true;
    bool m_config = false;
    std::map<std::string, bool> m_openWindows;
};

}  // namespace bee
