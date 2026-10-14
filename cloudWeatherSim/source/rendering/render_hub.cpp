#include "rendering/render_hub.hpp"

#include "cloud_hub.hpp"
#include "camera.hpp"

#include "rendering/new_open_gl.hpp"
#include "rendering/shapes_gl.hpp"
#include "rendering/helper_open_gl.hpp"
#include "rendering/draw_image.hpp"
#include "rendering/cuda/cuda_render_gl.h"
#include "rendering/cuda/cuda_render.cuh"
#include "rendering/cuda/cloudFile.cuh"

#include "cloud_menu_hub.hpp"
#include "config.h"
#include "utils.cuh"
#include "new_input.hpp"

#include <imgui/imgui.h>
#include <imgui/IconsFontAwesome.h>
#include <vector>
#include <glm/glm.hpp>

cloudRenderer::cloudRenderer() 
{ 
	m_shapesObj = new shapesGL();
	m_cudaRenderObj = new CudaRender();
	m_cloudFileObj = new cloudFile();
}

cloudRenderer::~cloudRenderer() 
{
    m_cudaRenderObj->cleanUp();

    delete m_cloudFileObj;
    delete m_cudaRenderObj;
	delete m_shapesObj;
}

void cloudRenderer::init(unsigned int framebuffer, unsigned int colorbuffer) 
{ 
    // Initialize Cloud Rendering OpenGLxCUDA
    if (m_cudaRenderObj)
    {
        m_cudaRenderObj->initOpenGLCUDAInterop(framebuffer, colorbuffer);
    }
    else printf("CLOUDLIB WARNING: tried to initialize the frame and color buffer, but m_cudaRenderObj was not initialized yet, did you call initialize() before setting the buffers?\n");
}

void cloudRenderer::render(float dt)
{
    // Update viewer
    gridDataSkyGPU* data = nullptr;
    bool updateSDF = m_cloudFileObj->updateViewing(data, dt);
    if (data) m_cudaRenderObj->setDataEnvironment(data->Qw, data->Qc, data->Qr, data->Qs, data->Qi, data->velfieldX, data->velfieldY, data->velfieldZ, updateSDF, getStream());
    
    // Call CUDA rendering
    m_cudaRenderObj->updateCameraSettings(CloudHub.CameraObj().getCamView(),
                                          CloudHub.CameraObj().getCamNear(),
                                          CloudHub.CameraObj().getCamFar());

    // Actually do the rendering
    m_cudaRenderObj->postRenderClouds();

    // Draw (Debug) lines and shapes
    m_shapesObj->render(CloudHub.CameraObj().getCamView(), CloudHub.CameraObj().getCamProjection());
}

void cloudRenderer::updateBuffers(unsigned int framebuffer, unsigned int colorbuffer) 
{
    m_cudaRenderObj->updateOnlyBuffers(framebuffer, colorbuffer);
}

void cloudRenderer::unregisterResources() 
{ 
    m_cudaRenderObj->unregisterResources();
}

void cloudRenderer::resize(int width, int height) 
{ 
	m_cudaRenderObj->setNewRenderSize(width, height); 
}

void cloudRenderer::renderMenuPanel(gameStates& currentState, const bool, bool&)
{
    switch (currentState)
    {
        case gameStates::VIEW_SIMULATION_SELECTION:

            chooseSavedSimulationRun(currentState);
            break;
        default:
            break;
    }
}

void cloudRenderer::renderPanel(gameStates& currentState, const bool initState, bool&)
{

    switch (currentState)
    {
    case gameStates::SIMULATION:

        ImGui::Begin("View Menu");

        if (initState)
        {
            // Initialize grid for rendering
            dim3 blockDim, gridDim;
            getGridBlockDims(gridDim, blockDim);
            m_cudaRenderObj->initEnvironmentData(GRIDSIZESKYX, GRIDSIZESKYY, GRIDSIZESKYZ, VOXELSIZE, gridDim, blockDim);
        }
        renderSettingsPanel();

        ImGui::End();

        m_cloudFileObj->panel();

        break;
    case gameStates::VIEW_SIMULATION:

        ImGui::Begin("View Menu");

        cloudViewMenu();
        renderSettingsPanel();

        ImGui::End();
        break;
    default:
        break;
    }

}

void cloudRenderer::renderSettingsPanel()
{
    if (ImGui::TreeNode("Render Info"))
    {
        bool changed = false;
        bool textureChanged = false;

        static renderSettings settings;

        if (ImGui::TreeNode("Noise Texture settings"))
        {
            ImGui::SliderFloat("persistence", &settings.persistence, 0, 1.0f);
            ImGui::SliderInt("gridSize", &settings.texGridSize, 1, 100);
            ImGui::SliderFloat("lacunarity", &settings.lacunarity, 1.0f, 32.0f);

            if (ImGui::Button("Generate Noise"))
            {
                textureChanged = true;
            }
            ImGui::TreePop();
        }

        ImGui::Separator();

        if (ImGui::TreeNode("Visual settings"))
        {
            if (ImGui::SliderFloat("NoiseReduction", &settings.noiseReduction, 0.0f, 5.0f)) changed = true;
            if (ImGui::SliderFloat("MinQw", &settings.minQw, 0.0f, 1.0f, "%.5f", ImGuiSliderFlags_Logarithmic)) changed = true;
            if (ImGui::SliderFloat("MaxQw", &settings.maxQw, 0.0f, 1.0f, "%.5f", ImGuiSliderFlags_Logarithmic)) changed = true;
            ImGui::Separator();
            if (ImGui::SliderFloat("multipleScattering", &settings.multipleScatteringDepthPower, 0.0f, 10.0f)) changed = true;
            if (ImGui::SliderFloat("Ambient Light Strength", &settings.ambientLightStrength, 0.0f, 1.0f)) changed = true;
            if (ImGui::SliderFloat("rayRandomOffset", &settings.rayRandomOffset, 0.0f, 1.0f)) changed = true;
            if (ImGui::SliderFloat("MS attenuation", &settings.attenuation, 0.001f, 1.0f)) changed = true;
            if (ImGui::SliderFloat("MS contribution", &settings.contribution, 0.001f, 1.0f)) changed = true;
            if (ImGui::SliderFloat("MS eccentricity attenuation", &settings.eccentricAttenuation, 0.001f, 1.0f)) changed = true;
            ImGui::Separator();
            if (ImGui::SliderFloat("Sun Strength", &settings.sunStrength, 1.0f, 255.0f)) changed = true;
            if (ImGui::SliderFloat("Light Exposure", &settings.exposure, 0.0f, 10.0f)) changed = true;
            if (ImGui::SliderFloat3("Sun Direction", settings.sunDirection, -1.0f, 1.0f)) changed = true;
            if (ImGui::ColorEdit3("Sun Color", settings.sunColor)) changed = true;

            ImGui::TreePop();
        }
        m_cloudFileObj->saveRenderSettingsPanel(settings);

        // Only updateInput when changed
        if (changed || textureChanged)
        {
            m_cudaRenderObj->setExtraRenderInfo(settings, textureChanged);
        }

        ImGui::TreePop();
    }
}

void cloudRenderer::cloudViewMenu() 
{

    // Variables
    static bool lerping = true;

    // Menu for the cloud viewer
    if (ImGui::Button(ICON_FA_PLAY))
    {
        m_cloudFileObj->m_cloudViewActive = true;
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_STOP))
    {
        m_cloudFileObj->m_cloudViewActive = false;
    }

    // Step forwards and step backwards
    if (ImGui::Button(ICON_FA_ARROW_LEFT) || CloudHub.InputObj().keyDown(newInput::LEFT_ARROW))
    {
        m_cloudFileObj->m_cloudViewStepOnce = -1;
    }
    ImGui::SameLine();

    if (ImGui::Button(ICON_FA_ARROW_RIGHT) || CloudHub.InputObj().keyDown(newInput::RIGHT_ARROW))
    {
        m_cloudFileObj->m_cloudViewStepOnce = 1;
    }

    float firstTime = m_cloudFileObj->getFirstFrameTime();
    float lastTime = m_cloudFileObj->getLastFrameTime();
    static float slideTimeInput = 0.0f;
    slideTimeInput = m_cloudFileObj->m_time - firstTime;
    if (ImGui::SliderFloat("Time", &slideTimeInput, 0.0f, lastTime - firstTime))
    {
        m_cloudFileObj->m_time = slideTimeInput + firstTime;
    }

    ImGui::Dummy(ImVec2(30, 30));

    // Calculate the current time of day from seconds
    const int hours = int(std::floor(m_cloudFileObj->m_time / (60.0f * 60.0f)));
    const int minutes = int(std::floor(m_cloudFileObj->m_time / 60.0f - (hours * 60.0f)));
    const int seconds = int(std::floor(m_cloudFileObj->m_time - (hours * 60.0f * 60.0f) - (minutes * 60.0f)));
    std::string time = std::to_string(hours) + ":" + std::to_string(minutes) + ":" + std::to_string(seconds);
    ImGui::Text("Time of day: %s", time.c_str());

    ImGui::Text("Speed Multiplier");
    ImGui::SliderFloat("##SpeedMult", &m_cloudFileObj->m_cloudViewSpeedMult, 1.0f, 10000.0f, "%.3f", ImGuiSliderFlags_Logarithmic);

    ImGui::Checkbox("Enable Lerping Frames", &lerping);
    ImGui::SetItemTooltip("Disabling lerping will snap simulaion to the nearest captured frame");

    // Type selection
    ImGui::BeginGroup();
    if (ImGui::TreeNode("Show Type"))
    {
        ImGui::Dummy(ImVec2(10, 10));
        ImGui::Separator();

        // If changed, we have to updateInput the visibility
        bool update{false};
        if (coloredSelectable("Water Cloud", &m_cloudFileObj->m_visibleTypes[0])) update = true;
        if (coloredSelectable("Ice Cloud", &m_cloudFileObj->m_visibleTypes[1])) update = true;
        if (coloredSelectable("Rain", &m_cloudFileObj->m_visibleTypes[2])) update = true;
        if (coloredSelectable("Snow", &m_cloudFileObj->m_visibleTypes[3])) update = true;
        if (coloredSelectable("Hail", &m_cloudFileObj->m_visibleTypes[4])) update = true;
        if (coloredSelectable("Water Vapor", &m_cloudFileObj->m_visibleTypes[5])) update = true;
        if (coloredSelectable("Wind", &m_cloudFileObj->m_visibleTypes[6])) update = true;
        if (update)
        {
            // Actually updateInput the visibility, but don't updateInput time (passing 0 for dt)
            gridDataSkyGPU* data = nullptr;
            m_cloudFileObj->updateViewing(data, 0.0f, true);
            if (data) m_cudaRenderObj->setDataEnvironment(data->Qw, data->Qc, data->Qr, data->Qs, data->Qi, data->velfieldX, data->velfieldY, data->velfieldZ, true, getStream());
        }

        ImGui::Dummy(ImVec2(10, 10));
        ImGui::TreePop();
    }
    ImGui::EndGroup();
    ImGui::SetItemTooltip("Show or hide different types of choosing");
}

void cloudRenderer::loadFile(const char* cloudFile)
{
    // First load the meta data from which we set the overal environment data
    cloudFileInfo info;
    info.loaded = m_cloudFileObj->getMetaData(cloudFile,
        info.sizeX,
        info.sizeY,
        info.sizeZ,
        info.voxelSize,
        info.totalFrames,
        info.types);
    if (!info.loaded) return; // Failed to load

    GRIDSIZESKYX = info.sizeX;
    GRIDSIZESKYY = info.sizeY;
    GRIDSIZESKYZ = info.sizeZ;
    VOXELSIZE = info.voxelSize;
    GRIDSIZESKY = GRIDSIZESKYX * GRIDSIZESKYY * GRIDSIZESKYZ;
    GRIDSIZEGROUND = GRIDSIZESKYX * GRIDSIZESKYZ;

    // Now load the actual data
    m_cloudFileObj->loadCloudFile(cloudFile, false);

    // Make every type visible and update by advancing 1 step
    for (int i = 0; i < 7; i++)
    {
        m_cloudFileObj->m_visibleTypes[i] = true;
    }
    m_cloudFileObj->m_cloudViewStepOnce = 1;

    // Get grid and blockdim from a cu file where we can check our specs
    dim3 blockDim;
    dim3 gridDim;
    getGridBlockDims(gridDim, blockDim);

    m_cloudFileObj->initGPUData(getStream());

    // Set faster render speed
    m_cudaRenderObj->setOnlyRenderResource(true);

    // Load viewer
    m_cudaRenderObj->initEnvironmentData(info.sizeX, info.sizeY, info.sizeZ, info.voxelSize, gridDim, blockDim);

    // Set camera Position based on grid
    glm::vec3 pos = glm::vec3(GRIDSIZESKYX, GRIDSIZESKYY, GRIDSIZESKYZ) * VOXELSIZE;
    glm::vec3 normPos = glm::normalize(pos);
    pos = normPos * distance(pos, glm::vec3(0.0f)) * 1.5f;
    glm::quat rotation = glm::quatLookAt(-normPos, glm::vec3(0, 1, 0));
    CloudHub.CameraObj().setCamPos(pos);
    CloudHub.CameraObj().setCamRot(rotation);
}


void cloudRenderer::loadRenderSettingsFile(const char* renderSettingsFile)
{
    renderSettings settings;
    if (m_cloudFileObj->loadSettingsFile(renderSettingsFile, settings))
    {
        m_cudaRenderObj->setExtraRenderInfo(settings, true);
    }
}

void cloudRenderer::chooseSavedSimulationRun(gameStates& currentState)
{
    // View all saved simulation

    static std::vector<std::string> simulationFiles = m_cloudFileObj->getSimulationFiles();
    static std::vector<cloudFileInfo> simulationFilesInfo(simulationFiles.size());

    // Using lambda to convert string to const char data
    auto getter = [](void* data, int idx, const char** outText) -> bool
    {
        auto& vec = *static_cast<std::vector<std::string>*>(data);
        *outText = vec[idx].c_str();
        return true;
    };

    ImGui::SameLine();
    if (ImGui::BeginTable("Files", 8))
    {
        // Introduction
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Loading");
        ImGui::TableNextColumn();
        ImGui::Text("File");
        ImGui::TableNextColumn();
        ImGui::Text("Simulation Size X");
        ImGui::TableNextColumn();
        ImGui::Text("Simulation Size Y");
        ImGui::TableNextColumn();
        ImGui::Text("Simulation Size Z");
        ImGui::TableNextColumn();
        ImGui::Text("Size of One Voxel");
        ImGui::TableNextColumn();
        ImGui::Text("Total Amount of Frames");
        ImGui::TableNextColumn();
        ImGui::Text("All Included Types");

        // List all files and note down meta data info if wanted to load
        for (int i = 0; i < simulationFiles.size(); i++)
        {
            std::string file = simulationFiles[i];
            cloudFileInfo& info = simulationFilesInfo[i];

            // Show all info
            char label[32];
            snprintf(label, sizeof(label), "Load Info ##%i", i);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            // Load meta data if button is pressed
            if (!info.loaded && ImGui::Button(label))
            {
                info.loaded = m_cloudFileObj->getMetaData(simulationFiles[i].c_str(),
                                                          info.sizeX,
                                                          info.sizeY,
                                                          info.sizeZ,
                                                          info.voxelSize,
                                                          info.totalFrames,
                                                          info.types);
            }
            ImGui::TableNextColumn();

            snprintf(label, sizeof(label), "File %s", file.c_str());
            static bool isSelected = false;
            if (ImGui::Selectable(label,
                                  isSelected,
                                  ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick))
            {
                if (ImGui::IsMouseDoubleClicked(0))
                {
                    loadFile(simulationFiles[i].c_str());
                    currentState = gameStates::VIEW_SIMULATION;
                }
            }
            ImGui::TableNextColumn();
            if (info.loaded) ImGui::Text(std::to_string(info.sizeX).c_str());
            ImGui::TableNextColumn();
            if (info.loaded) ImGui::Text(std::to_string(info.sizeY).c_str());
            ImGui::TableNextColumn();
            if (info.loaded) ImGui::Text(std::to_string(info.sizeZ).c_str());
            ImGui::TableNextColumn();
            if (info.loaded) ImGui::Text(std::to_string(info.voxelSize).c_str());
            ImGui::TableNextColumn();
            if (info.loaded) ImGui::Text(std::to_string(info.totalFrames).c_str());
            ImGui::TableNextColumn();
            if (info.loaded) ImGui::Text(info.types.c_str());
        }

        ImGui::EndTable();
    }
}
