#include "outside/simulating/simulation_hub.hpp"

#include "outside/simulating/readTable.h"
#include "outside/simulating/skewTMaker.h"
#include "outside/simulating/skewTFile.h"
#include "outside/simulating/cuda/environment.cuh"
#include "outside/simulating/simulationEditor.hpp"

#include "outside/cloud_hub.hpp"
#include "outside/cloud_menu_hub.hpp"
#include "outside/new_input.hpp"

#include <imgui/imgui.h>
#include <imgui/IconsFontAwesome.h>
#include <chrono>


simulationHub::simulationHub()
{
	m_readTableObj = new readTable();
    m_skewTMakerObj = new skewTMaker();
    m_skewTFileObj = new skewTFile();
    m_environmentObj = new environmentGPU();
    m_simulationEditObj = new simulationEditor();


}

simulationHub::~simulationHub() 
{
    // Join the thread
    m_running = false;
    if (simThread.joinable())
    {
        simThread.join();
    }

    delete m_simulationEditObj;
    delete m_environmentObj;
    delete m_skewTFileObj;
    delete m_skewTMakerObj;
	delete m_readTableObj;

}

void simulationHub::updateSimulation(float dt)
{

    // Update based on state
    if (m_gameState)
    {
        switch (*m_gameState)
        {
            case gameStates::SKEWT_CREATOR:

                m_skewTMakerObj->update(dt);

                break;
            case gameStates::SIMULATION:

                m_simulationEditObj->update(m_editing, m_skewT);

            default:
                break;
        }
    }

    // Check if the simulation needs to be updated
    // Update simulation thread variables
    if (m_simulationActive || m_simulationSteps > 0)
    {
        // Without simulation being active, we can still progress using the arrows
        if (m_simulationSteps > 0) m_simulationSteps--;

        m_speed.store(m_simulationSpeed);
    }
    else
    {
        // No speed means the simulation stops.
        m_speed.store(0.0f);
    }
}

void simulationHub::simulationMenuPanel(gameStates& currentState, const bool , bool& setInitState)
{
    m_gameState = &currentState;

    auto io = ImGui::GetIO();
    ImVec2 centerOfScreen = ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    ImVec2 returnButSize = ImVec2(100, 50);
    ImVec2 returnButLoc = ImVec2(io.DisplaySize.x - returnButSize.x - 10, io.DisplaySize.y - returnButSize.y - 10);

    switch (currentState)
    {
        case gameStates::MAINMENU:
        case gameStates::VIEW_SIMULATION_SELECTION:
        case gameStates::VIEW_SIMULATION:
            // We do nothing
            break;
        case gameStates::CREATE_SIMULATION:

            // Menu to all option to create a simulation, includes loading soundings
            ImGui::SetCursorPos(ImVec2(centerOfScreen.x - 175, centerOfScreen.y - 250));
            if (ImGui::Button("Create Skew-T", ImVec2(350, 100)))
            {
                m_skewTMakerObj->init();
                setInitState = true;
                currentState = gameStates::SKEWT_CREATOR;
            }
            ImGui::SetCursorPos(ImVec2(centerOfScreen.x - 175, centerOfScreen.y - 75));
            if (ImGui::Button("Load Observed Skew-T", ImVec2(350, 100)))
            {
                m_skewTFileObj->init();
                chooseObservedSounding(true);
                setInitState = true;
                currentState = gameStates::OBSERVED_SOUNDING_SELECTION;
            }
            ImGui::SetCursorPos(ImVec2(centerOfScreen.x - 175, centerOfScreen.y + 100));
            if (ImGui::Button("Load Custom Skew-T", ImVec2(350, 100)))
            {
                setInitState = true;
                currentState = gameStates::CUSTOM_ENVIRONMENT_SELECTION;
            }

            ImGui::SetCursorPos(returnButLoc);
            if (ImGui::Button("Back", returnButSize)) currentState = gameStates::MAINMENU, setInitState = true;
            
            break;
        case gameStates::OBSERVED_SOUNDING_SELECTION:

            // Make player choose from year, month and day
            if (chooseObservedSounding(false))
            {
                m_skewTMakerObj->init();
                setInitState = true;
                currentState = gameStates::SKEWT_CREATOR;
            }

            ImGui::SetCursorPos(returnButLoc);
            if (ImGui::Button("Back", returnButSize)) currentState = gameStates::CREATE_SIMULATION, setInitState = true;
            
            break;
        case gameStates::CUSTOM_ENVIRONMENT_SELECTION:

            // Nothing here yet...

            ImGui::SetCursorPos(returnButLoc);
            if (ImGui::Button("Back", returnButSize)) currentState = gameStates::CREATE_SIMULATION, setInitState = true;
            
            break;

            break;

        case gameStates::SKEWT_CREATOR:
        case gameStates::SIMULATION:

            break;

        default:
            break;
    }


}

void simulationHub::simulationPanel(gameStates& currentState, const bool initState, bool& setInitState)
{
    switch (currentState)
    {
        case gameStates::SKEWT_CREATOR:
            if (m_skewTMakerObj->skewTMakerPanel())
            {
                m_simulationEditObj->init();
                setInitState = true;
                currentState = gameStates::SIMULATION;
            }
            break;
        case gameStates::SIMULATION:
            if (initState)
            {
                if (!m_simulationInitialized)
                {
                    // Create a new thread on which we will simulate
                    setSimulation();
                    m_simulationInitialized = true;
                }
            }

            simulationMainPanel();
            break;
        default:
            break;
    }
}


void simulationHub::simulationMainPanel() 
{
    static bool dataViewer = false;

    // Main simulation menu
    if (ImGui::Button(ICON_FA_PLAY))
    {
        m_simulationActive = true;
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_STOP))
    {
        m_simulationActive = false;
    }
    ImGui::SameLine();
    ImGui::Dummy(ImVec2(30, 30));
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_ARROW_RIGHT) || CloudHub.InputObj().keyDown(newInput::RIGHT_ARROW))
    {
        m_simulationSteps = 1;
    }


    ImGui::BeginGroup();
    std::string mode = m_editing ? "View" : "Edit";
    if (ImGui::Button(mode.c_str()))
    {
        m_editing = m_editing ? false : true;
    }
    ImGui::SameLine();
    if (ImGui::Button("SkewT"))
    {
        m_skewT = m_skewT ? false : true;
    }
    if (ImGui::Button("Data"))
    {
        dataViewer = dataViewer ? false : true;
    }
    ImGui::EndGroup();

    // Settings
    ImGui::SliderFloat("Simulation Speed", &m_simulationSpeed, 0.1f, 100.0f);
    if (ImGui::Button("Reset Speed")) m_simulationSpeed = 1.0f;


    if (m_editing) m_simulationEditObj->editPanel();
    m_simulationEditObj->viewPanel();
    if (m_skewT) m_simulationEditObj->skewTPanel();
    if (dataViewer) m_simulationEditObj->dataPanel(m_simulationActive);
}

void simulationHub::setSimulation()
{
    m_running = true;

    simThread = std::thread(
        [this]()
        {
            // Initialize deltatime at about 1 ms or larger
            auto time = std::chrono::high_resolution_clock::now();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            float accumulator = 0.0f;
            const float fixedDt = 1.0f / 15.0f;  // Fps we want to target
            auto prevTime = std::chrono::high_resolution_clock::now();

            while (m_running)
            {
                float speed = m_speed.load();
                if (speed > 0.0f)
                {
                    // Calculate deltatime based on time taken for previous updateInput
                    auto ctime = std::chrono::high_resolution_clock::now();
                    float dt = std::chrono::duration<float>(ctime - prevTime).count();
                    prevTime = ctime;
                    dt = std::min(dt, m_maxDeltaTimeSimulation);

                    accumulator += dt;

                    // Make sure accumulator does not stack up higher and higher
                    accumulator = std::min(accumulator, fixedDt * 5.0f);

                    // Make sure the simulation does not run faster than it needs to be
                    while (accumulator >= fixedDt)
                    {
                        // If slow, pass the current dt to speed up the simulation to real time.
                        const float passedDt = std::min(fixedDt, dt);
                        m_environmentObj->updateGPU(passedDt, speed);
                        accumulator -= passedDt;
                    }
                }
                else
                {
                    accumulator = 0.0f;
                }

                // Sleep to increase dt, else it will round to 0, meaning accumulator will never add up
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });
}

bool simulationHub::chooseObservedSounding(bool init)
{
    static std::vector<std::string> availableYears;
    static std::vector<std::string> availableMonths;
    static std::vector<std::string> availableDays;
    static std::vector<fileInfo> availableFiles;

    bool confirmed = false;

    // Small initialization part
    if (init)
    {
        availableYears.clear();
        m_skewTFileObj->getAvailableYears(availableYears);
        return false;
    }

    // Using lambda to convert string to const char data
    auto getter = [](void* data, int idx, const char** outText) -> bool
    {
        auto& vec = *static_cast<std::vector<std::string>*>(data);
        *outText = vec[idx].c_str();
        return true;
    };

    static int currentYear = -1;
    ImGui::PushItemWidth(ImGui::CalcTextSize("  0000  ").x);
    if (ImGui::ListBox("##Year", &currentYear, getter, &availableYears, int(availableYears.size()), 10))
    {
        m_skewTFileObj->getAvailableMonths(availableYears[currentYear], availableMonths);
    }
    ImGui::SameLine();

    static int currentMonth = -1;
    if (ImGui::ListBox("##Month", &currentMonth, getter, &availableMonths, int(availableMonths.size()), 12))
    {
        m_skewTFileObj->getAvailableDays(availableYears[currentYear],
                                                         availableMonths[currentMonth],
                                                         availableDays);
    }
    ImGui::SameLine();

    static int currentDay = -1;
    static int fileAmount = 0;
    static std::vector<bool> selected(fileAmount, false);
    if (ImGui::ListBox("##Day", &currentDay, getter, &availableDays, int(availableDays.size()), 10))
    {
        m_skewTFileObj->getAvailableFiles(availableYears[currentYear],
                                                          availableMonths[currentMonth],
                                                          availableDays[currentDay],
                                                          availableFiles);
        fileAmount = 0;
        for (fileInfo& file : availableFiles)
        {
            for (int i = 0; i < int(file.dates.size()); i++)
            {
                fileAmount++;
            }
        }
        selected.resize(fileAmount);
        std::fill(selected.begin(), selected.end(), false);
    }

    ImGui::PopItemWidth();

    ImGui::SameLine();
    if (ImGui::BeginTable("Files", 5))
    {
        // Introduction
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("File #");
        ImGui::TableNextColumn();
        ImGui::Text("Country");
        ImGui::TableNextColumn();
        ImGui::Text("Station");
        ImGui::TableNextColumn();
        ImGui::Text("Date");

        // Actual selectables
        int count = 0;
        for (fileInfo& file : availableFiles)
        {
            for (std::string& date : file.dates)
            {
                // Show all info
                count++;
                char label[32];
                snprintf(label, sizeof(label), "File %d", count);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                bool isSelected = selected[count - 1];
                if (ImGui::Selectable(label,
                                      isSelected,
                                      ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick))
                {
                    std::fill(selected.begin(), selected.end(), false);
                    selected[count - 1] = true;
                    if (ImGui::IsMouseDoubleClicked(0))
                    {
                        m_skewTFileObj->openAndReadFile(file, date);
                        confirmed = true;
                    }
                }
                ImGui::TableNextColumn();
                ImGui::Text(file.country.c_str());
                ImGui::TableNextColumn();
                ImGui::Text(file.station.c_str());
                ImGui::TableNextColumn();
                ImGui::Text(date.c_str());
            }
        }
        ImGui::EndTable();
    }
    return confirmed;
}