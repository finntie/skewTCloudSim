//#if defined(BEE_INSPECTOR)
#include "tools/inspector.hpp"
#include <imgui/imgui.h>
#include <imgui/imgui_impl.h>
#include <imgui/implot.h>
#include <glm/gtc/type_ptr.hpp>
#include "core/ecs.hpp"
#include "core/engine.hpp"
#include "core/device.hpp"
#include "core/fileio.hpp"
#include "core/input.hpp"
#include "core/transform.hpp"
#include "tools/log.hpp"
#include "tools/tools.hpp"
#include "tools/profiler.hpp"
#include "tools/inspectable.hpp"
#include "rendering/debug_render.hpp"
#include <imgui/IconsFontAwesome.h>
#include <imgui/imgui_internal.h>
#include "tools/warnings.hpp"

#include <rendering/render_components.hpp>
#include <glm/gtx/matrix_decompose.hpp>

using namespace bee;
using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////
////										Helper stuff
////////////////////////////////////////////////////////////////////////////////////////////////

static void SetStyle();

namespace
{

ImVec4 GetColor(const glm::vec3& color) { return ImVec4(color.r, color.g, color.b, 1.0); }

void AddToInspected(Entity entity, std::set<Entity>& inspected)
{
    inspected.insert(entity);
    if (auto* transform = Engine.ECS().Registry.try_get<Transform>(entity))
    {
        for (auto child : *transform)
        {
            AddToInspected(child, inspected);
        }
    }
}

void DrawRowsBackground(float line_height, float x1, float x2, float y_offset, ImU32, ImU32)
{
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    float y0 = ImGui::GetCursorScreenPos().y + (float)(int)y_offset;

    const auto pos = ImGui::GetCursorPos();

    const float scroll_y = ImGui::GetScrollY();
    const int first_visible_row = (int)floor(scroll_y / line_height);
    const int row_count = (int)round(ImGui::GetWindowHeight() / line_height) + 1;
    const ImU32 darken = IM_COL32(0, 0, 0, 30);

    for (int row_n = first_visible_row; row_n < (first_visible_row + row_count); ++row_n)
    {
        if (row_n & 1) continue;
        float y1 = y0 + (line_height * static_cast<float>(row_n));
        float y2 = y1 + line_height;
        draw_list->AddRectFilled(ImVec2(x1, y1), ImVec2(x2, y2), darken);
    }

    ImGui::SetCursorPos(pos);
}

float cToolbarSpacing = 4.0f;

}  // namespace

SceneInspector::SceneInspector() {}

void SceneInspector::OnPanel()
{
    static ImGuiTextFilter filter;
    filter.Draw(ICON_FA_SEARCH);
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_TIMES))
    {
        filter.Clear();
    }
    Inspector::Tooltip("Clear filter");

    // Make the child window 1/3 of the height
    float height = ImGui::GetWindowHeight();
    float width = ImGui::GetWindowWidth();
    // Make edge to edge
    auto pos = ImGui::GetCursorPos();
    pos.x = 0;
    ImGui::SetCursorPos(pos);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
    ImGui::BeginChild("Hierarchy", {width, height * 0.3f});

    float x1 = ImGui::GetCurrentWindow()->WorkRect.Min.x;
    float x2 = ImGui::GetCurrentWindow()->WorkRect.Max.x;
    float item_spacing_y = ImGui::GetStyle().ItemSpacing.y;
    float item_offset_y = -item_spacing_y * 0.5f;
    float line_height = ImGui::GetTextLineHeight() + item_spacing_y;
    DrawRowsBackground(line_height, x1, x2, item_offset_y, 0, ImGui::GetColorU32(ImVec4(0.4f, 0.4f, 0.4f, 0.1f)));

    // Filter the entities
    std::set<Entity> inspected_or_filtered;
    auto entities = Engine.ECS().Registry.view<Transform>();
    Filter(entities, inspected_or_filtered, filter);

    Engine.ECS().Registry.view<Transform>().each(
        [this, &inspected_or_filtered](auto entity, Transform& transform)
        {
            if (!transform.HasParent()) this->Inspect(entity, transform, inspected_or_filtered);
        });
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    string selectedEntityName = Engine.ECS().Registry.valid(m_selectedEntity)
                                    ? (Engine.ECS().Registry.get<Transform>(m_selectedEntity).Name + "   " + ICON_FA_ID_CARD +
                                       " " + to_string(static_cast<int>(m_selectedEntity)))
                                    : "None";
    ImGui::SeparatorText(selectedEntityName.c_str());
    if (Engine.ECS().Registry.valid(m_selectedEntity))
    {
        ImGui::PushID((void*)m_selectedEntity);
        if (ImGui::Button(ICON_FA_TRASH)) Engine.ECS().DeleteEntity(m_selectedEntity);
        Inspector::Tooltip("Delete");
        for (auto editor : IEntityInspector::m_editors) editor->OnEntity(m_selectedEntity);

        ImGui::PopID();
    }
}

void SceneInspector::OnEntity(entt::entity entity)
{
    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (Engine.ECS().Registry.try_get<Transform>(entity))
        {
            Transform& t = Engine.ECS().Registry.get<Transform>(entity);
            glm::vec3 translation(t.GetTranslation());
            if (DragFloat3("Position", translation, 0.01f)) t.SetTranslation(translation);
            glm::vec3 scale(t.GetScale());
            if (DragFloat3("Scale", scale, 0.01f)) t.SetScale(scale);
            glm::vec3 rotationEuler(glm::eulerAngles(t.GetRotation()));
            if (DragFloat3("Rotation (Euler)", rotationEuler, 0.01f)) t.SetRotation(glm::quat(rotationEuler));
        }
    }
}

bool bee::SceneInspector::ComponentHeader(const char* label, ImGuiTreeNodeFlags flags)
{
    auto pos = ImGui::GetCursorPos();
    pos.x = 0;
    ImGui::SetCursorPos(pos);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    auto open = ImGui::CollapsingHeader(label, flags);
    ImGui::PopStyleVar();
    return open;
}

void bee::SceneInspector::ManipToolbar(ImVec2 pos)
{
    if (m_selectedEntity != entt::null)
    {
        unsigned flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoScrollWithMouse;

        ImGui::Begin("Manipulator Toolbar 0", nullptr, flags);
        ImGui::SetWindowPos(pos);
        ImGui::SetWindowSize({-1, -1});

        auto selectedColor = ImVec4(0.0f, 0.0f, 0.0f, 0.5f);
        auto unselectedColor = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

        auto color = m_gizmoOperation == ImGuizmo::TRANSLATE ? selectedColor : unselectedColor;
        ImGui::PushStyleColor(ImGuiCol_Button, color);
        if (ImGui::Button(ICON_FA_ARROWS_ALT)) m_gizmoOperation = ImGuizmo::TRANSLATE;
        ImGui::PopStyleColor();
        Inspector::Tooltip("Translate");
        ImGui::SameLine();

        color = m_gizmoOperation == ImGuizmo::ROTATE ? selectedColor : unselectedColor;
        ImGui::PushStyleColor(ImGuiCol_Button, color);
        if (ImGui::Button(ICON_FA_REPEAT)) m_gizmoOperation = ImGuizmo::ROTATE;
        ImGui::PopStyleColor();
        Inspector::Tooltip("Rotate");
        ImGui::SameLine();

        color = m_gizmoOperation == ImGuizmo::SCALE ? selectedColor : unselectedColor;
        ImGui::PushStyleColor(ImGuiCol_Button, color);
        if (ImGui::Button(ICON_FA_EXPAND)) m_gizmoOperation = ImGuizmo::SCALE;
        ImGui::PopStyleColor();
        Inspector::Tooltip("Scale");
        ImGui::SameLine();
        auto width = ImGui::GetWindowWidth();
        ImGui::End();

        pos.x += width + 4.0f;
        ImGui::Begin("Manipulator Toolbar 1", nullptr, flags);
        ImGui::SetWindowPos(pos);
        ImGui::SetWindowSize({-1, -1});

        // Gizmo mode as buttons as well
        color = m_gizmoMode == ImGuizmo::LOCAL ? selectedColor : unselectedColor;
        ImGui::PushStyleColor(ImGuiCol_Button, color);
        if (ImGui::Button(ICON_FA_CUBE)) m_gizmoMode = ImGuizmo::LOCAL;
        ImGui::PopStyleColor();
        Inspector::Tooltip("Local");
        ImGui::SameLine();

        color = m_gizmoMode == ImGuizmo::WORLD ? selectedColor : unselectedColor;
        ImGui::PushStyleColor(ImGuiCol_Button, color);
        if (ImGui::Button(ICON_FA_GLOBE)) m_gizmoMode = ImGuizmo::WORLD;
        ImGui::PopStyleColor();
        Inspector::Tooltip("World");

        ImGui::End();
    };
}

void bee::SceneInspector::Gizmo(const glm::mat4& view, const glm::mat4& projection)
{
    if (m_selectedEntity == entt::null) return;
    auto* transform = Engine.ECS().Registry.try_get<Transform>(m_selectedEntity);
    if (!transform) return;

    auto model = transform->World();

    ImGuizmo::SetRect(0, 0, ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y);
    ImGuizmo::Manipulate(glm::value_ptr(view),
                         glm::value_ptr(projection),
                         m_gizmoOperation,
                         m_gizmoMode,
                         glm::value_ptr(model));

    if (ImGuizmo::IsUsing())
    {
        if (transform->HasParent())  // transform to local space
        {
            auto& parentTransform = Engine.ECS().Registry.get<Transform>(transform->GetParent());
            model = glm::inverse(parentTransform.World()) * model;
        }
        transform->SetFromMatrix(model);
    }
}

void SceneInspector::Inspect(Entity entity, Transform& transform, std::set<Entity>& inspected)
{
    if (inspected.find(entity) != inspected.end()) return;
    inspected.insert(entity);

    string name = transform.Name.empty() ? "Entity-" + std::to_string(static_cast<std::uint32_t>(entity)) : transform.Name;

    ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

    auto ptr = reinterpret_cast<void*>(static_cast<long long>(entity));

    if (entity == m_selectedEntity) nodeFlags |= ImGuiTreeNodeFlags_Selected;

    if (transform.HasChildren())
    {
        bool nodeOpen = ImGui::TreeNodeEx(ptr, nodeFlags, "%s", name.c_str());
        if (ImGui::IsItemClicked()) m_selectedEntity = entity;

        if (nodeOpen)
        {
            for (auto child : transform)
            {
                if (Engine.ECS().Registry.valid(child))
                {
                    auto& childTransform = Engine.ECS().Registry.get<Transform>(child);
                    Inspect(child, childTransform, inspected);
                }
            }
            ImGui::TreePop();
        }
        else
        {
            for (auto child : transform) AddToInspected(child, inspected);
        }
    }
    else
    {
        ImGui::TreeNodeEx(ptr,
                          nodeFlags | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_Bullet,
                          "%s",
                          name.c_str());
        if (ImGui::IsItemClicked()) m_selectedEntity = entity;
    }
}

bool SceneInspector::DragFloat3(const char* label,
                                glm::vec3& v,
                                float speed,
                                float min,
                                float max,
                                const char* format,
                                ImGuiSliderFlags flags)
{
    // Used ImGui but with a differently colored frame outline per component (x, y, z)
    ImGui::PushID(label);
    ImGui::BeginGroup();
    ImGui::PushItemWidth((ImGui::CalcItemWidth() - 6) / 3.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
    bool ret = false;
    ret |= ImGui::DragFloat("##x", &v.x, speed, min, max, format, flags);
    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
    ImGui::PopStyleColor();
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.2f, 1.0f, 0.2f, 1.0f));
    ret |= ImGui::DragFloat("##y", &v.y, speed, min, max, format, flags);
    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
    ImGui::PopStyleColor();
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.4f, 0.4f, 1.0f, 1.0f));
    ret |= ImGui::DragFloat("##z", &v.z, speed, min, max, format, flags);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::PopItemWidth();
    ImGui::EndGroup();
    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
    ImGui::TextUnformatted(label);
    ImGui::PopID();
    return ret;
}

void SceneInspector::Filter(
    entt::basic_view<entt::get_t<entt::sigh_mixin<entt::basic_storage<Transform>>>, entt::exclude_t<>>& entities,
    std::set<entt::entity>& inspected_or_filtered,
    const ImGuiTextFilter& filter)
{
    // Fist go through all entities and filter them
    for (auto entity : entities)
    {
        auto& transform = entities.get<Transform>(entity);
        if (!filter.PassFilter(transform.Name.c_str())) inspected_or_filtered.insert(entity);
    }

    // Next walk through all entities and remove their entire parent and grandparents from the filtered set
    for (auto entity : entities)
    {
        auto& transform = entities.get<Transform>(entity);
        if (inspected_or_filtered.find(entity) == inspected_or_filtered.end())
        {
            auto parent = transform.GetParent();
            while (parent != entt::null)
            {
                inspected_or_filtered.erase(parent);
                parent = entities.get<Transform>(parent).GetParent();
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////
////										Inspector
////////////////////////////////////////////////////////////////////////////////////////////////
Inspector::Inspector()
{
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGui_Impl_Init();

    ImGuiIO& io = ImGui::GetIO();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    const std::string iniPath = Engine.FileIO().GetPath(FileIO::Directory::SaveFiles, "imgui.ini");
    const char* constStr = iniPath.c_str();
    char* str = new char[iniPath.size() + 1];
    strcpy_s(str, iniPath.size() + 1, constStr);
    io.IniFilename = str;

    const float UIScale = Engine.Device().GetMonitorUIScale();
    const float fontSize = 14.0f;
    const float iconSize = 14.0f;

    ImFontConfig config;
    config.OversampleH = 8;
    config.OversampleV = 8;
    io.Fonts->AddFontFromFileTTF(Engine.FileIO().GetPath(FileIO::Directory::SharedAssets, "/fonts/DroidSans.ttf").c_str(),
                                 fontSize * UIScale,
                                 &config);
    config.MergeMode = true;
    config.OversampleH = 8;
    config.OversampleV = 8;

    static const ImWchar icon_ranges[] = {ICON_MIN_FA, ICON_MAX_FA, 0};

    string fontpath = Engine.FileIO().GetPath(FileIO::Directory::SharedAssets, "/fonts/FontAwesome5FreeSolid900.otf");
    io.Fonts->AddFontFromFileTTF(fontpath.c_str(), iconSize * UIScale, &config, icon_ranges);

    SetStyle();
    m_openWindows["Configuration"] = false;

    InitFromFile();
}

Inspector::~Inspector()
{
    ImGui_Impl_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    SaveToFile();
}

void bee::Inspector::SaveToFile()
{
    nlohmann::json j;
    j["Inspector"] = m_openWindows;

    std::ofstream ofs;
    auto filename = Engine.FileIO().GetPath(FileIO::Directory::SaveFiles, "inspector.json");
    ofs.open(filename);
    if (ofs.is_open())
    {
        auto str = j.dump(4);
        ofs << str;
        ofs.close();
    }
}

void bee::Inspector::InitFromFile()
{
    std::ifstream ifs;
    auto filename = Engine.FileIO().GetPath(FileIO::Directory::SaveFiles, "inspector.json");
    ifs.open(filename);
    if (ifs.is_open())
    {
        nlohmann::json j = nlohmann::json::parse(ifs);
        m_openWindows = j["Inspector"];
    }
}

bool bee::Inspector::IsMouseOver() { return ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) || ImGui::IsAnyItemHovered(); }

bool bee::Inspector::IsSelected() { return m_selectedTab; }

void Inspector::Inspect(float)
{
    ImGui_Impl_NewFrame();
    ImGuizmo::BeginFrame();

    Toolbar();
    Panel();
    Stats();
    Gizmo();
    Selected();

    ImGui::Render();
    ImGui_Impl_RenderDrawData(ImGui::GetDrawData());
}

bool Inspector::Inspect(const char* name, float& f) { return ImGui::DragFloat(name, &f, 0.01f); }

bool Inspector::Inspect(const char* name, int& i) { return ImGui::DragInt(name, &i); }

bool Inspector::Inspect(const char* name, bool& b) { return ImGui::Checkbox(name, &b); }

bool Inspector::Inspect(const char* name, glm::vec2& v) { return ImGui::DragFloat2(name, glm::value_ptr(v)); }

bool Inspector::Inspect(const char* name, glm::vec3& v)
{
    if (StringEndsWith(string(name), "Color") || StringEndsWith(string(name), "color"))
    {
        return ImGui::ColorEdit3(name, glm::value_ptr(v), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
    }
    else
    {
        return ImGui::DragFloat3(name, glm::value_ptr(v));
    }
}

bool Inspector::Inspect(const char* name, glm::vec4& v)
{
    if (StringEndsWith(string(name), "Color") || StringEndsWith(string(name), "color"))
    {
        return ImGui::ColorEdit4(name, glm::value_ptr(v), ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
    }
    else
    {
        return ImGui::DragFloat4(name, glm::value_ptr(v));
    }
}

void Inspector::Tooltip(const char* text)
{
    if (ImGui::IsItemHovered() && GImGui->HoveredIdTimer > 0.6f)
    {
        ImGui::BeginTooltip();
        ImGui::SetTooltip("%s", text);
        ImGui::EndTooltip();
    }
}

void Inspector::Toolbar()
{
    const float spacing = 4.0f;
    // Push the colors from above
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f, 1.00f, 1.00f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.00f, 0.00f, 0.00f, 0.45f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.00f, 0.00f, 0.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.00f, 1.00f, 1.00f, 0.46f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.00f, 0.00f, 0.00f, 1.00f));

    // Push some styles
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 5.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 5.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    // Draw the most basic toolbar
    unsigned flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::Begin("Toolbar 0", nullptr, flags);
    ImGui::SetWindowPos({spacing, spacing});
    ImGui::SetWindowSize({-1, -1});
    /*ImGui::Button(ICON_FA_BARS);
    Tooltip("File");
    ImGui::SameLine();
    ImGui::Button(ICON_FA_PAUSE);
    Tooltip("Pause");
    ImGui::SameLine();*/
    if (Engine.Device().CanClose())
    {
        if (ImGui::Button(ICON_FA_STOP)) Engine.Device().RequestClose();
        Tooltip("Stop");
    }

    ImGui::SameLine();
    bool debug = ImGui::Button(ICON_FA_BUG);
    Tooltip("Debug Rendering");
    if (debug) ImGui::OpenPopup("Debug Render");
    if (ImGui::BeginPopup("Debug Render"))
    {
        auto debugRenderFlags = Engine.DebugRenderer().GetCategoryFlags();
        bool changed = false;
        changed |= ImGui::CheckboxFlags("General", &debugRenderFlags, DebugCategory::General);
        changed |= ImGui::CheckboxFlags("Gameplay", &debugRenderFlags, DebugCategory::Gameplay);
        changed |= ImGui::CheckboxFlags("Physics", &debugRenderFlags, DebugCategory::Physics);
        changed |= ImGui::CheckboxFlags("AI Navigation", &debugRenderFlags, DebugCategory::AINavigation);
        changed |= ImGui::CheckboxFlags("AI Decision Making", &debugRenderFlags, DebugCategory::AIDecision);
        changed |= ImGui::CheckboxFlags("Sound", &debugRenderFlags, DebugCategory::Sound);
        changed |= ImGui::CheckboxFlags("Rendering", &debugRenderFlags, DebugCategory::Rendering);
        changed |= ImGui::CheckboxFlags("Editor", &debugRenderFlags, DebugCategory::Editor);
        changed |= ImGui::CheckboxFlags("Acceleration Struct", &debugRenderFlags, DebugCategory::AccelStructs);
        changed |= ImGui::CheckboxFlags("Grid", &debugRenderFlags, DebugCategory::Grid);
        if (changed)
        {
            Engine.DebugRenderer().SetCategoryFlags(debugRenderFlags);
            // SaveToFile();
        }
        ImGui::EndMenu();
    }

    ImGui::SameLine();
    for (const auto& i : IToolbar::m_editors)
    {
        i->OnToolbar();
        ImGui::SameLine();
    }
    float width = ImGui::GetWindowWidth();
    ImGui::End();

    m_scene.ManipToolbar({width + spacing * 2.0f, spacing});

    // Pop the colors and styles
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(5);
}

void Inspector::Panel()
{
    ImGui::Begin(": : : : : :", nullptr, ImGuiWindowFlags_MenuBar);

    ImVec4* colors = ImGui::GetStyle().Colors;
    ImGui::PushStyleColor(ImGuiCol_Header, colors[ImGuiCol_WindowBg]);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, colors[ImGuiCol_ButtonHovered]);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {8, 4});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {8, 12});

    ImGui::BeginMenuBar();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
    int i = 0;
    for (const auto& panel : IPanel::m_editors)
    {
        const auto& icon = panel->GetIcon();
        const auto& name = panel->GetName();
        auto color = ImGui::GetColorU32(GetColor(RandomNiceColor(i)));

        if (i != 0) ImGui::PushStyleColor(ImGuiCol_Text, color);
        auto p = ImGui::GetCursorScreenPos();
        ImGui::Begin(icon.c_str());
        if (i != 0) ImGui::PopStyleColor();
        IPanel::m_editors[i]->OnPanel();
        ImGui::End();
        //bool selected = m_selectedPanel == i;
        //if (ImGui::MenuItem(icon.c_str(), nullptr, &selected)) m_selectedPanel = i;
        Tooltip(name.c_str());
        i++;
    }
    ImGui::EndMenuBar();


    ImGui::End();
}

void Inspector::Stats()
{
    // A small transparent window at the bottom of the screen

    // Get the window size from ImGui
    auto size = ImGui::GetIO().DisplaySize;
    // Set the position to the bottom left corner
    auto pos = ImVec2(0, size.y);
    // Create a window with the size of the window
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always, ImVec2(0, 1));
    ImGui::SetNextWindowSize(ImVec2(size.x, 0), ImGuiCond_Always);
    // Set the window to be transparent(ish) and the text to be white
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.25f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

    // Create a window with no title bar, no resize, no move, no saved settings, no focus on appearing, and no nav
    ImGui::Begin("Stats",
                 nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);

    // Get the frame rate from ImGui
    auto frameRate = ImGui::GetIO().Framerate;
    // Get the frame time from ImGui
    auto frameTime = ImGui::GetIO().DeltaTime;

    // Display the frame rate and frame time

    ImGui::Text("%s %s | %s %3.1f | %s %2.2f ms",
                ICON_FA_TAG,
                Engine.GetVersionString().c_str(),
                ICON_FA_DESKTOP,
                frameRate,
                ICON_FA_CLOCK_O,
                frameTime * 1000.0f);

    for (auto editor : IStatsBar::m_editors)
    {
        ImGui::SameLine();
        ImGui::Text("|");
        ImGui::SameLine();
        editor->OnStatsBar();
    }

    // Pop the color
    ImGui::PopStyleColor(2);

    // End the window
    ImGui::End();
}

void bee::Inspector::Gizmo()
{
    auto cameras = Engine.ECS().Registry.view<Camera>();
    if (cameras.empty()) return;
    auto cameraEntity = *cameras.begin();
    auto& cameraTransform = Engine.ECS().Registry.get<Transform>(cameraEntity);
    auto& camera = Engine.ECS().Registry.get<Camera>(*cameras.begin());

    auto view = glm::inverse(cameraTransform.World());
    auto projection = camera.Projection;

    m_scene.Gizmo(view, projection);
}

void bee::Inspector::Selected() 
{
    if (Engine.Input().GetMouseButtonOnce(Input::MouseButton::Left))
    {
        if (IsMouseOver())
        {
            // The current window is focused
            ImVec4* colors = ImGui::GetStyle().Colors;
            colors[ImGuiCol_WindowBg] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
            colors[ImGuiCol_Button] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
            colors[ImGuiCol_TitleBg] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
            colors[ImGuiCol_MenuBarBg] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
            colors[ImGuiCol_Header] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
            m_selectedTab = true;
        }
        else
        {
            ImVec4* colors = ImGui::GetStyle().Colors;
            colors[ImGuiCol_WindowBg] = ImVec4(0.22f, 0.22f, 0.22f, 0.30f);
            colors[ImGuiCol_Button] = ImVec4(0.12f, 0.12f, 0.12f, 0.30f);
            colors[ImGuiCol_TitleBg] = ImVec4(0.06f, 0.06f, 0.06f, 0.70f);
            colors[ImGuiCol_MenuBarBg] = ImVec4(0.06f, 0.06f, 0.06f, 0.30f);
            colors[ImGuiCol_Header] = ImVec4(0.12f, 0.12f, 0.12f, 0.30f);

            m_selectedTab = false;
        }
    }

}

void SetStyle()
{
    // Main
    auto* style = &ImGui::GetStyle();
    style->FrameRounding = 5.0f;
    style->WindowPadding = ImVec2(10.0f, 10.0f);
    style->FramePadding = ImVec2(8.0f, 5.0f);
    style->ItemSpacing = ImVec2(10.0f, 4.0f);
    style->IndentSpacing = 12;
    style->ScrollbarSize = 12;
    style->GrabMinSize = 9;

    // Sizes
    style->WindowBorderSize = 0.0f;
    style->ChildBorderSize = 0.0f;
    style->PopupBorderSize = 0.0f;
    style->FrameBorderSize = 0.0f;
    style->TabBorderSize = 0.0f;

    style->WindowRounding = 4.0f;
    style->ChildRounding = 4.0f;
    style->FrameRounding = 4.0f;
    style->PopupRounding = 4.0f;
    style->GrabRounding = 2.0f;
    style->ScrollbarRounding = 12.0f;
    style->TabRounding = 6.0f;
    style->WindowMenuButtonPosition = ImGuiDir_None;
    style->WindowTitleAlign = ImVec2(0.5f, 0.5f);

    ImVec4* colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_Text] = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.09f, 0.09f, 0.09f, 0.60f);
    colors[ImGuiCol_Border] = ImVec4(0.06f, 0.06f, 0.06f, 0.31f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.16f, 0.17f, 0.18f, 0.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.36f, 0.36f, 0.37f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.54f, 0.54f, 0.54f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.06f, 0.06f, 0.06f, 0.40f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.13f, 0.14f, 0.16f, 0.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.51f, 0.51f, 0.51f, 0.52f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.69f, 0.69f, 0.69f, 0.55f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(1.00f, 1.00f, 1.00f, 0.75f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.90f, 0.90f, 0.90f, 0.50f);
    colors[ImGuiCol_SliderGrab] = ImVec4(1.00f, 1.00f, 1.00f, 0.30f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.37f, 0.37f, 0.37f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.37f, 0.37f, 0.37f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.06f, 0.06f, 0.06f, 0.20f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.37f, 0.37f, 0.37f, 1.00f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.19f, 0.19f, 0.19f, 1.00f);
    colors[ImGuiCol_TabSelected] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_TabSelectedOverline] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_TabDimmed] = ImVec4(0.13f, 0.14f, 0.16f, 1.00f);
    colors[ImGuiCol_TabDimmedSelected] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(0.50f, 0.50f, 0.50f, 0.00f);
    colors[ImGuiCol_DockingPreview] = ImVec4(0.50f, 0.50f, 0.50f, 0.00f);
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotLines] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
    colors[ImGuiCol_TextLink] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.72f, 0.34f, 0.00f, 1.00f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(0.72f, 0.34f, 0.00f, 1.00f);
    colors[ImGuiCol_NavCursor] = ImVec4(0.72f, 0.34f, 0.00f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);
}

//#else

//#include "tools/inspector.hpp"
//
//using namespace std;
//using namespace bee;
//
//Inspector::Inspector() {}
//Inspector::~Inspector() {}
//bool Inspector::IsMouseOver() { return false; }
//void Inspector::Inspect(float) {}
//bool Inspector::Inspect(const char*, float&) { return false; }
//bool Inspector::Inspect(const char*, int&) { return false; }
//bool Inspector::Inspect(const char*, bool&) { return false; }
//bool Inspector::Inspect(const char*, glm::vec2&) { return false; }
//bool Inspector::Inspect(const char*, glm::vec3&) { return false; }
//bool Inspector::Inspect(const char*, glm::vec4&) { return false; }

//#endif  // BEE_INSPECTOR
