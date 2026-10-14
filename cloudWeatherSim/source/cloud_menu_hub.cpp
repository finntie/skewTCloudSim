#include "cloud_menu_hub.hpp"

#include "cloud_hub.hpp"
#include "new_input.hpp"
#include "camera.hpp"

#include "simulating/simulation_hub.hpp"
#include "simulating/skewTFile.h"

#include "rendering/render_hub.hpp"


#include "imgui/imgui.h"
#include <imgui/implot.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>
#include <imgui/IconsFontAwesome.h>

#include <string>


#include <filesystem>


cloudMenu::cloudMenu(simulationHub* simHubObj, cloudRenderer* renderHubObj, bool customStyle)
{
    m_simHubObj = simHubObj;
    m_renderHubObj = renderHubObj;
    if (customStyle) initializeImGuiStyle();
}

cloudMenu::~cloudMenu()
{

}


void cloudMenu::initializeImGuiStyle()
{
    // Initialize fonts and style

    if (ImGui::GetCurrentContext() == nullptr)
    {
        printf("CLOUDLIB MESSAGE: Unable to set custom ImGui style; no ImGui context found, is ImGui already setup before calling this library's init?\n");
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Check if the files actually exists
    std::filesystem::path checkPath = std::filesystem::current_path() / "assets/ImGui/ImGui.ini";

    if (!std::filesystem::exists(checkPath)) { printf("CLOUDLIB WARNING: could not find the path: %s\n", checkPath.string().c_str()); return; }
    checkPath = std::filesystem::current_path() / "assets/ImGui/fonts/DroidSans.ttf";
    if (!std::filesystem::exists(checkPath)) { printf("CLOUDLIB WARNING: could not find the path: %s\n", checkPath.string().c_str()); return; }
    checkPath = std::filesystem::current_path() / "assets/ImGui/fonts/FontAwesome5FreeSolid900.otf";
    if (!std::filesystem::exists(checkPath)) { printf("CLOUDLIB WARNING: could not find the path: %s\n", checkPath.string().c_str()); return; }

    std::string filePath = "assets/ImGui/ImGui.ini";
    const char* constStr = filePath.c_str();
    char* str = new char[filePath.size() + 1];
    strcpy_s(str, filePath.size() + 1, constStr);
    io.IniFilename = str;

    const float fontSize = 14.0f;
    const float iconSize = 14.0f;

    filePath = "assets/ImGui/fonts/DroidSans.ttf";
    ImFontConfig config;
    config.OversampleH = 8;
    config.OversampleV = 8;
    io.Fonts->AddFontFromFileTTF(filePath.c_str(), fontSize, &config);
    config.MergeMode = true;
    config.OversampleH = 8;
    config.OversampleV = 8;

    static const ImWchar icon_ranges[] = {ICON_MIN_FA, ICON_MAX_FA, 0};

    filePath = "assets/ImGui/fonts/FontAwesome5FreeSolid900.otf";
    io.Fonts->AddFontFromFileTTF(filePath.c_str(), iconSize, &config, icon_ranges);

    imguiStyle();
}


void cloudMenu::update() 
{
    // Check if ImGui is set up correctly
    if (ImGui::GetCurrentContext() == nullptr)
    {
        printf("CLOUDLIB ERROR: Unable to use ImGui; no ImGui context found, is ImGui setup correctly?\n");
        return;
    }

    bool changedState = false;

    switch (m_currentState)
    {
        case gameStates::MAINMENU:
        case gameStates::CREATE_SIMULATION:
        case gameStates::OBSERVED_SOUNDING_SELECTION:
        case gameStates::CUSTOM_ENVIRONMENT_SELECTION:
        case gameStates::VIEW_SIMULATION_SELECTION:

            startMenu(m_gameInitStateChange, changedState);
            break;

        case gameStates::SKEWT_CREATOR:
        case gameStates::SIMULATION:
        case gameStates::VIEW_SIMULATION:

            otherMenus(m_gameInitStateChange, changedState);
            break;

        default:
            break;
    }

    m_gameInitStateChange = changedState;
}

void cloudMenu::otherMenus(const bool initState, bool& setInitState)
{
    ImGui::Begin("main");

    // Check if panel is focused or not
    checkPanelSelection();

    // Simulation Panel
    m_simHubObj->simulationPanel(m_currentState, initState, setInitState);
    // Render Panel
    m_renderHubObj->renderPanel(m_currentState, initState, setInitState);

    ImGui::End();
}

void cloudMenu::checkPanelSelection()
{
    m_panelHovered = (ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) || ImGui::IsAnyItemHovered() || ImGui::IsAnyItemActive());

    // Set panel selected if clicking on panel, or unset if clicking not on a panel
    if (m_panelHovered && CloudHub.InputObj().mouseOnce(newInput::MOUSE_LEFT)) m_panelSelected = true;
    else if (m_panelSelected && !m_panelHovered && CloudHub.InputObj().mouseOnce(newInput::MOUSE_LEFT))
    {
        m_panelSelected = false;
        CloudHub.CameraObj().enable();
    }

    if (m_panelSelected)
    {
        // The current window is focused
        ImVec4* colors = ImGui::GetStyle().Colors;
        colors[ImGuiCol_WindowBg] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
        colors[ImGuiCol_MenuBarBg] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);

        // Always disable camera
        CloudHub.CameraObj().disable();
    }
    else
    {
        // Reduce alpha if not focused
        ImVec4* colors = ImGui::GetStyle().Colors;
        colors[ImGuiCol_WindowBg] = ImVec4(0.22f, 0.22f, 0.22f, 0.30f);
        colors[ImGuiCol_Button] = ImVec4(0.12f, 0.12f, 0.12f, 0.30f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.06f, 0.06f, 0.06f, 0.70f);
        colors[ImGuiCol_MenuBarBg] = ImVec4(0.06f, 0.06f, 0.06f, 0.30f);
        colors[ImGuiCol_Header] = ImVec4(0.12f, 0.12f, 0.12f, 0.30f);
    }
}

void cloudMenu::startMenu(const bool initState, bool& setInitState)
{ 
    styleStartMenu(true);

    // Set window settings
    ImGui::SetWindowPos(ImVec2(0, 0));
    auto io = ImGui::GetIO();
    ImGui::SetWindowSize(io.DisplaySize);
    ImGui::SetWindowFocus();
    ImVec2 centerOfScreen = ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    ImVec2 returnButSize = ImVec2(100, 50);
    ImVec2 returnButLoc = ImVec2(io.DisplaySize.x - returnButSize.x - 10, io.DisplaySize.y - returnButSize.y - 10);

    switch (m_currentState)
    {
        case gameStates::MAINMENU:

            // Actual buttons
            ImGui::SetCursorPos(ImVec2(centerOfScreen.x - 175, centerOfScreen.y - 200));
            if (ImGui::Button("Create Simulation", ImVec2(350, 100)))
            {
                setInitState = true;
                m_currentState = gameStates::CREATE_SIMULATION;
            }
            ImGui::SetCursorPos(ImVec2(centerOfScreen.x - 175, centerOfScreen.y));
            if (ImGui::Button("View Simulation", ImVec2(350, 100)))
            {
                setInitState = true;
                m_currentState = gameStates::VIEW_SIMULATION_SELECTION;
            }

            break;
        case gameStates::CREATE_SIMULATION:
        case gameStates::OBSERVED_SOUNDING_SELECTION:
        case gameStates::CUSTOM_ENVIRONMENT_SELECTION:

            m_simHubObj->simulationMenuPanel(m_currentState, initState, setInitState);

            // Handles back button itself

            break;
        case gameStates::VIEW_SIMULATION_SELECTION:

            m_renderHubObj->renderMenuPanel(m_currentState, initState, setInitState);

            ImGui::SetCursorPos(returnButLoc);
            if (ImGui::Button("Back", returnButSize)) m_currentState = gameStates::MAINMENU, setInitState = true;

            break;
        default:
            break;
    }

    styleStartMenu(false);
}

void cloudMenu::styleStartMenu(bool init) 
{
    if (init)
    {
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.3f, 0.3f, 0.7f, 1.0f));

        // Start the window
        ImGui::Begin("StartMenu", 0, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar);


        // Button customization
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1, 1, 1, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.45f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.55f, 0.4f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.3f, 0.15f, 1.0f));

        // ListBox customization
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.1f, 0.45f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.2f, 0.55f, 0.4f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.1f, 0.3f, 0.15f, 1.0f));

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);
    }
    else
    {
        // Pop styles that we added
        ImGui::PopStyleColor(7);
        ImGui::PopStyleVar(2);

        ImGui::End();

        ImGui::PopStyleColor();
    }
}

void cloudMenu::setImGuiContext()
{
    m_PrevImGuiContext = ImGui::GetCurrentContext();
    m_PrevImPlotContext = ImPlot::GetCurrentContext();
    ImGui::SetCurrentContext(m_ImGuiContext);
    ImPlot::SetCurrentContext(m_ImPlotContext);
}

void cloudMenu::unsetImGuiContext() 
{
    // Set users context back
    if (m_PrevImGuiContext) ImGui::SetCurrentContext(m_PrevImGuiContext);
    if (m_PrevImPlotContext) ImPlot::SetCurrentContext(m_PrevImPlotContext);
}

void cloudMenu::imguiStyle() 
{
    // Stolen from BEE engine
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
