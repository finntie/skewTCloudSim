#include "simulating/simulationEditor.hpp"


#include "cloud_hub.hpp"
#include "new_input.hpp"
#include "cloud_menu_hub.hpp"
#include "utils.cuh"

#include "simulating/simulation_hub.hpp"
#include "simulating/skewTer.h"
#include "simulating/tracing.h"
#include "simulating/cuda/environment.cuh"
#include "simulating/cuda/dataClass.cuh"

#include "rendering/draw_image.hpp"
#include "camera.hpp"
#include "rendering/colors.hpp"
#include "rendering/render_hub.hpp"
#include "rendering/shapes_gl.hpp"
#include "rendering/cuda/cloudFile.cuh"

#include "math/meteoformulas.cuh"
#include "math/geometry.hpp"

#include <imgui/imgui.h>
#include <imgui/ImGuizmo.h>
#include <glm/glm.hpp>
#include <algorithm>
#include <glm/gtc/type_ptr.hpp>

simulationEditor::simulationEditor() 
{ 
    m_envEditData = new envDebugData();
    m_colorSchemeObj = new colorScheme();
    m_skewTObj = new skewTer();
    m_dataClassObj = new dataClass();
    m_tracingObj = new tracing();

    setColorScheme();
    setSliceMinMax(false); 
}

simulationEditor::~simulationEditor() 
{ 
    delete m_tracingObj;
    delete m_dataClassObj;
    delete m_skewTObj;
    delete m_colorSchemeObj;
    delete m_envEditData;
}

void simulationEditor::init() 
{
    m_envEditData->init(GRIDSIZESKYX, GRIDSIZESKYY, GRIDSIZESKYZ);
    GPUSetEnv(true);
    initializeSkewT();

    m_tracingObj->init();

    // Set camera Position based on grid
    glm::vec3 pos = glm::vec3(GRIDSIZESKYX, GRIDSIZESKYY, GRIDSIZESKYZ) * VOXELSIZE;
    glm::vec3 normPos = glm::normalize(pos);
    pos = normPos * distance(pos, glm::vec3(0.0f)) * 1.5f;
    glm::quat rotation = glm::quatLookAt(-normPos, glm::vec3(0,1,0));
    CloudHub.CameraObj().setCamPos(pos);
    CloudHub.CameraObj().setCamRot(rotation);
}

void simulationEditor::setColorScheme()
{
    // ColorSchemes
    m_colorSchemeObj->createColorScheme("TemperatureSky", -40 + 273.15f, Colors::Blue, 40 + 273.15f, Colors::Red);
    m_colorSchemeObj->addColor("TemperatureSky", -10 + 273.15f, Colors::Cyan);
    m_colorSchemeObj->addColor("TemperatureSky", 10 + 273.15f, Colors::Green);
    m_colorSchemeObj->addColor("TemperatureSky", 20 + 273.15f, Colors::Yellow);

    m_colorSchemeObj->createColorScheme("mixingRatio", 0.0f, Colors::Blue, 1.0f, Colors::Red);
    m_colorSchemeObj->addColor("mixingRatio", 0.0005f, Colors::Cyan);
    m_colorSchemeObj->addColor("mixingRatio", 0.001f, Colors::Green);
    m_colorSchemeObj->addColor("mixingRatio", 0.01f, Colors::Yellow);

    m_colorSchemeObj->createColorScheme("velField", 0, Colors::Blue, 100, Colors::Red);
    m_colorSchemeObj->addColor("velField", 10, Colors::Cyan);
    m_colorSchemeObj->addColor("velField", 25, Colors::Green);
    m_colorSchemeObj->addColor("velField", 50, Colors::Yellow);

    m_colorSchemeObj->createColorScheme("debugColor", -1, Colors::Purple * 0.2f, 1, Colors::White);
    m_colorSchemeObj->addColor("debugColor", -0.01f, Colors::Purple);
    m_colorSchemeObj->addColor("debugColor", -0.01f, Colors::Blue);
    m_colorSchemeObj->addColor("debugColor", -0.001f, Colors::DodgerBlue);
    m_colorSchemeObj->addColor("debugColor", -0.0001f, Colors::Cyan);
    m_colorSchemeObj->addColor("debugColor", 0.0f, Colors::Green);
    m_colorSchemeObj->addColor("debugColor", 0.0001f, Colors::Yellow);
    m_colorSchemeObj->addColor("debugColor", 0.001f, Colors::Orange);
    m_colorSchemeObj->addColor("debugColor", 0.01f, Colors::Red);
    m_colorSchemeObj->addColor("debugColor", 0.1f, Colors::Pink + glm::vec3(0, 0.8f, 0));

    m_colorSchemeObj->createColorScheme("realistic", 0.0f, Colors::Grey, 1.0f, Colors::Black);
    m_colorSchemeObj->addColor("realistic", 0.0005f, glm::vec3(0.6f, 0.7f, 0.8f));
    m_colorSchemeObj->addColor("realistic", 0.001f, Colors::White);
    m_colorSchemeObj->addColor("realistic", 0.005f, glm::vec3(0.8f, 0.8f, 0.8f));
    m_colorSchemeObj->addColor("realistic", 0.01f, glm::vec3(0.4f, 0.4f, 0.4f));

    m_colorSchemeObj->createColorScheme("pressure", 0.0f, Colors::Purple, 1050.0f, Colors::White);
    m_colorSchemeObj->addColor("pressure", 100.0f, glm::vec3(0.3f, 0.0f, 0.75f));
    m_colorSchemeObj->addColor("pressure", 200.0f, glm::vec3(0.1f, 0.8f, 0.9f));
    m_colorSchemeObj->addColor("pressure", 300.0f, glm::vec3(0.0f, 0.8f, 1.0f));
    m_colorSchemeObj->addColor("pressure", 400.0f, glm::vec3(0.0f, 0.4f, 1.0f));
    m_colorSchemeObj->addColor("pressure", 600.0f, glm::vec3(0.0f, 0.8f, 0.8f));
    m_colorSchemeObj->addColor("pressure", 700.0f, glm::vec3(0.0f, 1.0f, 0.0f));
    m_colorSchemeObj->addColor("pressure", 800.0f, glm::vec3(0.4f, 0.8f, 0.0f));
    m_colorSchemeObj->addColor("pressure", 900.0f, glm::vec3(0.8f, 0.0f, 0.0f));
    m_colorSchemeObj->addColor("pressure", 950.0f, glm::vec3(1.0f, 0.6f, 0.6f));
    m_colorSchemeObj->addColor("pressure", 975.0f, glm::vec3(0.6f, 0.5f, 0.5f));
    m_colorSchemeObj->addColor("pressure", 1000.0f, glm::vec3(0.2f, 0.2f, 0.2f));

    m_colorSchemeObj->createColorScheme("density", -1, Colors::Purple * 0.0f, 1, Colors::White);
    m_colorSchemeObj->addColor("density", 0.15f, Colors::Purple);
    m_colorSchemeObj->addColor("density", 0.3f, Colors::Blue);
    m_colorSchemeObj->addColor("density", 0.5f, Colors::DodgerBlue);
    m_colorSchemeObj->addColor("density", 0.75f, Colors::Cyan);
    m_colorSchemeObj->addColor("density", 0.9f, Colors::Green);
    m_colorSchemeObj->addColor("density", 1.0f, Colors::Yellow);
    m_colorSchemeObj->addColor("density", 1.1f, Colors::Orange);
    m_colorSchemeObj->addColor("density", 1.2f, Colors::Red);
    m_colorSchemeObj->addColor("density", 1.225f, Colors::Pink + glm::vec3(0, 0.8f, 0));
}

void simulationEditor::updateValues() 
{
    // Mouse pos index
    const int pointingAtIdx = m_tracingObj->getVoxelAtMouse();
    if (pointingAtIdx == -1)
    {
        m_selectionInGrid = false;
    }
    else
    {
        m_selectionInGrid = true;
        m_mousePointingIndex = pointingAtIdx;  // Do not change if invalid index
    }
    getCoord(m_mousePointingIndex, m_mousePointingPos.x, m_mousePointingPos.y, m_mousePointingPos.z);

    // If space is pressed, updateInput skewT index
    if (CloudHub.InputObj().keyDown(newInput::SPACEBAR))
    {
        m_skewTidx = m_mousePointingIndex;
        getCoord(m_skewTidx, m_skewTPos.x, m_skewTPos.y, m_skewTPos.z);
    }
}

void simulationEditor::initializeSkewT() 
{
    // Initialize skewTer
    {
        float* temps = new float[GRIDSIZESKYY];
        float* dewPoints = new float[GRIDSIZESKYY];

        dataToSkewTData(temps, dewPoints);

        skewTer::skewTInfo skewT;
        skewT.init(GRIDSIZESKYY, temps, dewPoints, m_envEditData->m_envPressure);
        m_skewTidx = (m_envEditData->m_groundHeight[0] + 1) * GRIDSIZESKYX;
        m_skewTObj->setSkewT(skewT);

        delete[] temps;
        delete[] dewPoints;
    }
}

void simulationEditor::dataToSkewTData(float* temp, float* dew)
{
    const int x = m_skewTPos.x;
    const int y = m_skewTPos.y;
    const int z = m_skewTPos.z;

    // Use i for our y lookup, gathering all data up into the sky
    for (int i = 0; i < GRIDSIZESKYY; i++)
    {
        if (i < y)
        {
            temp[i] = 0.0f;
            dew[i] = 0.0f;
            continue;
        }
        const int idx = getIdx(x, i, z);
        const int idxG = x + z * GRIDSIZESKYX;

        const float Tz = float(m_envEditData->m_envView.potTemp[idx]) - 273.15f;
        const float T = MForms::potentialTemp(Tz, m_envEditData->m_groundView.P[idxG], m_envEditData->m_envView.pressure[idx]);

        const float rs = MForms::ws(T, m_envEditData->m_envView.pressure[idx]);
        const float RH = m_envEditData->m_envView.Qv[idx] / rs * 100;
        // Dew point calculation https://www.omnicalculator.com/physics/dew-point
        float dewpoint = 0.0f;
        {
            const float a = 17.625f;
            const float b = 243.04f;
            const float c = log(RH / 100) + a * T / (b + T);
            dewpoint = (b * c) / (a - c);
        }

        temp[i] = T;
        dew[i] = RH == 0 ? 0.0f : dewpoint;
    }
}


void simulationEditor::update(bool editing, bool skewT) 
{
    // Update values
    updateValues();

    // Retrieve data
    CloudHub.CloudSim().Environment().getDebugArrayValues(*m_envEditData);
    GPUSetEnv(false);

    // Handle input
    controls();

    // Update editing values
    updateEditing(editing);

    // Update skewtTer
    updateSkewT(skewT);

    // View debug data
    m_tracingObj->resetGrid(false);
    viewDebugSky();
    viewDebugGround();

    // Render call to possibly create a save frame if recording
    CloudHub.CloudRender().CloudFile().tryCreateFrame(m_envEditData->m_envView, m_envEditData->m_groundView, m_time);

    // View cursor
    CloudHub.CloudRender().shapesGLObj().AddVoxel((glm::vec3(m_mousePointingPos) + 0.5f) * VOXELSIZE, VOXELSIZE, Colors::WhiteA);

    // Mouse/camera cursor
    glm::vec3 mousepos3D = CloudHub.CameraObj().getMousePos3D();
    CloudHub.CloudRender().shapesGLObj().AddCircle(mousepos3D, 0.5f, glm::vec3(0, 1, 0), glm::vec4(1.0));
    CloudHub.CloudRender().shapesGLObj().AddFilledSquare(mousepos3D, 0.5f, glm::vec3(0, 1, 0), glm::vec4(1.0));

    // Reset values
    m_mouseWheel = CloudHub.InputObj().mouseScroll();
}



void simulationEditor::updateEditing(bool editMode)
{
    if (editMode || m_microPhysDataSelect)
    {
        if (editMode)
        {
            viewBrush();
            applyBrush();
            usePicker();
        }
        viewSelect();
        applySelect();
    }
    else if (m_justViewSelection)
    {
        viewSelect();
    }
}

void simulationEditor::updateSkewT(bool skewTView) 
{
    if (skewTView)
    {
        // Small SkewT Index check
        int idxG = m_skewTPos.x + m_skewTPos.z * GRIDSIZESKYX;
        if (m_skewTPos.y <= m_envEditData->m_groundHeight[idxG])
        {
            // Update new coordinate to be above ground
            int newY = m_envEditData->m_groundHeight[idxG] + 1;
            m_skewTPos.y = newY < GRIDSIZESKYY ? newY : GRIDSIZESKYY - 1;
            m_skewTidx = getIdx(m_skewTPos.x, m_skewTPos.y, m_skewTPos.z);
        }

        // Copy over data at the skewT idx to our skewT class
        float* temps = new float[GRIDSIZESKYY];
        float* dewPoints = new float[GRIDSIZESKYY];
        dataToSkewTData(temps, dewPoints);

        m_skewTObj->setAllArrays(temps, dewPoints, m_envEditData->m_envPressure);


        m_skewTObj->setStartHeight(m_skewTPos.y);
        m_skewTObj->drawSkewT();

        delete[] temps;
        delete[] dewPoints;
    }
}


void simulationEditor::controls() 
{
    // -----------------------Update camera when holding shift---------------------

    if (m_brushing || m_selecting)
    {
        if (CloudHub.InputObj().keyDown(newInput::LEFT_SHIFT)) CloudHub.CameraObj().enable();
        else CloudHub.CameraObj().disable();
    }

    // -----------------------Update camera when holding shift---------------------

    if (CloudHub.InputObj().mouseOnce(newInput::MOUSE_RIGHT))
    {
        if (m_selecting) m_selecting = false;
        if (m_justViewSelection) m_justViewSelection = false;
    }


    //------------------------------------------------------------------------------
    //-------------------------Scroll through slices--------------------------------
    //------------------------------------------------------------------------------

    if (m_viewSlice && CloudHub.InputObj().keyDown(newInput::LEFT_CONTROL) && m_mouseWheel != CloudHub.InputObj().mouseScroll())
    {
        const int scroll = int(m_mouseWheel) - int(CloudHub.InputObj().mouseScroll());
        int sliceMaxCoord = GRIDSIZESKYX - 1;
        if (m_viewSliceCoord == 0) sliceMaxCoord = GRIDSIZESKYX - 1;
        else if (m_viewSliceCoord == 1) sliceMaxCoord = GRIDSIZESKYY - 1;
        else if (m_viewSliceCoord == 2) sliceMaxCoord = GRIDSIZESKYZ - 1;

        m_atSliceViewSlice = std::clamp(m_atSliceViewSlice + scroll, 0, sliceMaxCoord);
        setSliceMinMax(!m_viewSlice);
    }
}

// ----------------------------------------------------- UI -----------------------------------------------------


void simulationEditor::viewPanel()
{
    // Panel for viewing the parameters, also includes simulation info
    viewParamValuesPanel();

    // Shortcut panels, shows tooltips
    shortCutViewDataPanel();

    ImGui::Begin("View Params");
    {
        viewParamsPanel();
        slicePanel();
    }
    ImGui::End();
}

void simulationEditor::editPanel() 
{
    ImGui::Begin("Edit Variables");
    {
        editParamsPanel();
    }
    ImGui::End();
}

void simulationEditor::skewTPanel() 
{
    ImGui::Begin("SkewT");
    {
        skewTTexturePanel();
        setSkewTDataPanel();  // Settings below skewTTexture
    }
    ImGui::End();
}

void simulationEditor::dataPanel(bool simActive) 
{ 
    ImGui::Begin("Data Viewer");
    {
        dataSettingsPanel();
        dataViewPanel(simActive);
    }
    ImGui::End();
}

void simulationEditor::viewParamValuesPanel() 
{
    int x = m_mousePointingPos.x;
    int y = m_mousePointingPos.y;
    int z = m_mousePointingPos.z;
    const int Gidx = x + z * GRIDSIZESKYX;

    ImGui::Indent();
    ImGui::Text(std::string("X: " + std::to_string(x) + ", Y: " + std::to_string(y) + ", Z: " + std::to_string(z)).c_str());
    ImGui::Text(std::string("Meters Per Voxel: " + std::to_string(VOXELSIZE)).c_str());
    ImGui::Text(
        "--Sky--\nPot Temp: \nQv: \nQw: \nQc: \nQr: \nQs: \nQi: \nWind: \nPressure: \nDebug1: \nDebug2: \nDebug3: "
        "\n--Ground--\nTemp: \nWater: \nQr: \nQs: \nQi: ");
    ImGui::SameLine();
    ImGui::Text(("\n" +
                 std::to_string(m_envEditData->m_envView.potTemp[m_mousePointingIndex] - 273.15) + "\n" +
                 std::to_string(m_envEditData->m_envView.Qv[m_mousePointingIndex]) + "\n" +
                 std::to_string(m_envEditData->m_envView.Qw[m_mousePointingIndex]) + "\n" +
                 std::to_string(m_envEditData->m_envView.Qc[m_mousePointingIndex]) + "\n" +
                 std::to_string(m_envEditData->m_envView.Qr[m_mousePointingIndex]) + "\n" +
                 std::to_string(m_envEditData->m_envView.Qs[m_mousePointingIndex]) + "\n" +
                 std::to_string(m_envEditData->m_envView.Qi[m_mousePointingIndex]) + "\n" +
                 std::to_string(m_envEditData->m_envView.velFieldX[m_mousePointingIndex]) + ", " +
                 std::to_string(m_envEditData->m_envView.velFieldY[m_mousePointingIndex]) + ", " +
                 std::to_string(m_envEditData->m_envView.velFieldZ[m_mousePointingIndex]) + "\n" +
                 std::to_string(m_envEditData->m_envView.pressure[m_mousePointingIndex]) + "\n" +
                 std::to_string(m_envEditData->m_debugArray0[m_mousePointingIndex]) + "\n" +
                 std::to_string(m_envEditData->m_debugArray1[m_mousePointingIndex]) + "\n" +
                 std::to_string(m_envEditData->m_debugArray2[m_mousePointingIndex]) + "\n" + "\n" +  // Ground
                 std::to_string(m_envEditData->m_groundView.T[Gidx] - 273.15) + "\n" +
                 std::to_string(m_envEditData->m_groundView.Qrs[Gidx]) + "\n" + 
                 std::to_string(m_envEditData->m_groundView.Qgr[Gidx]) + "\n" + 
                 std::to_string(m_envEditData->m_groundView.Qgs[Gidx]) + "\n" +
                 std::to_string(m_envEditData->m_groundView.Qgi[Gidx]) + "\n")
                    .c_str());
}

void simulationEditor::viewParamsPanel() 
{
    if (ImGui::TreeNode("Sky"))
	{
		ImGui::Text("View parameter of:");
		if (ImGui::Button("Temp")) m_viewParamSky = POTTEMP;	ImGui::SameLine();
		if (ImGui::Button("Wind")) m_viewParamSky = WINDX;	ImGui::SameLine();
		if (ImGui::Button("Ps"))   m_viewParamSky = PRESSURE;		ImGui::SameLine();
		if (ImGui::Button("Qv"))   m_viewParamSky = QV;		ImGui::SameLine();
		if (ImGui::Button("Qw"))   m_viewParamSky = QW;
		if (ImGui::Button("Qc"))   m_viewParamSky = QC;		ImGui::SameLine();
		if (ImGui::Button("Qr"))   m_viewParamSky = QR;		ImGui::SameLine();
		if (ImGui::Button("Qs"))   m_viewParamSky = QS;		ImGui::SameLine();
		if (ImGui::Button("Qi"))   m_viewParamSky = QI;

		if (ImGui::Button("Debug1")) m_viewParamSky = DEBUG1;	  ImGui::SameLine();
		if (ImGui::Button("Debug2")) m_viewParamSky = DEBUG2;	  ImGui::SameLine();
		if (ImGui::Button("Debug3")) m_viewParamSky = DEBUG3;

		ImGui::TreePop();
	}
	if (ImGui::TreeNode("Ground"))
	{
		ImGui::Text("View parameter of:");
		if (ImGui::Button("Temp")) m_viewParamGround = TEMP;			ImGui::SameLine();
		if (ImGui::Button("WaterContent")) m_viewParamGround = QRS;
		if (ImGui::Button("Qr")) m_viewParamGround = QGR;				ImGui::SameLine();
		if (ImGui::Button("Qs")) m_viewParamGround = QGS;				ImGui::SameLine();
		if (ImGui::Button("Qi")) m_viewParamGround = QGI;

		ImGui::TreePop();
	}
	//Resetting
	if (ImGui::TreeNode("Resetting"))
	{
		ImGui::Text("Reset one parameter:");
        if (ImGui::Button("Temp")) lockGlobal(), CloudHub.CloudSim().Environment().resetParameterGPU(POTTEMP), unlockGlobal(); ImGui::SameLine();
		if (ImGui::Button("Wind")) lockGlobal(), CloudHub.CloudSim().Environment().resetParameterGPU(WINDX), unlockGlobal(); ImGui::SameLine();
		if (ImGui::Button("Ps")) lockGlobal(), CloudHub.CloudSim().Environment().resetParameterGPU(PRESSURE), unlockGlobal();
		if (ImGui::Button("Qv")) lockGlobal(), CloudHub.CloudSim().Environment().resetParameterGPU(QV), unlockGlobal(); ImGui::SameLine();
		if (ImGui::Button("Qw")) lockGlobal(), CloudHub.CloudSim().Environment().resetParameterGPU(QW), unlockGlobal(); ImGui::SameLine();
		if (ImGui::Button("Qc")) lockGlobal(), CloudHub.CloudSim().Environment().resetParameterGPU(QC), unlockGlobal();
		if (ImGui::Button("Qr")) lockGlobal(), CloudHub.CloudSim().Environment().resetParameterGPU(QR), unlockGlobal(); ImGui::SameLine();
		if (ImGui::Button("Qs")) lockGlobal(), CloudHub.CloudSim().Environment().resetParameterGPU(QS), unlockGlobal(); ImGui::SameLine();
		if (ImGui::Button("Qi")) lockGlobal(), CloudHub.CloudSim().Environment().resetParameterGPU(QI), unlockGlobal();

		ImGui::TreePop();
	}

}

void simulationEditor::slicePanel() 
{
    if (ImGui::TreeNode("View-Slice"))
    {
        static int sliceMaxCoord = GRIDSIZESKYX - 1;

        if (ImGui::Checkbox("Use Slice View", &m_viewSlice))
        {
            setSliceMinMax(!m_viewSlice);
        }

        if (ImGui::RadioButton("Slice X", &m_viewSliceCoord, 0))
        {
            setSliceMinMax(!m_viewSlice);
            sliceMaxCoord = GRIDSIZESKYX - 1;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Slice Y", &m_viewSliceCoord, 1))
        {
            setSliceMinMax(!m_viewSlice);
            sliceMaxCoord = GRIDSIZESKYY - 1;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Slice Z", &m_viewSliceCoord, 2))
        {
            setSliceMinMax(!m_viewSlice);
            sliceMaxCoord = GRIDSIZESKYZ - 1;
        }

        if (ImGui::SliderInt("Slice Layer", &m_atSliceViewSlice, 0, sliceMaxCoord))
        {
            setSliceMinMax(!m_viewSlice);
        }

        ImGui::Checkbox("Show Ground", &m_viewGround);
        ImGui::Checkbox("Show Sky", &m_viewSky);
        ImGui::Checkbox("WireFrame", &m_wireFrame);

        ImGui::TreePop();
    }
}

void simulationEditor::editParamsPanel() 
{
    // Edit parameters 

	if (ImGui::TreeNode("Diurnal Cycle"))
    {
        // Time sliderfloat
        float timeHour = m_time / 3600.0f;
        if (ImGui::SliderFloat("Time of day (H)", &timeHour, 0.0f, 24.0f))
        {
            m_timeChanged = true;
        }
        m_time = timeHour * 3600.0f;
        // Pause day cyclus
        ImGui::Checkbox("Pause Diurnal Cycle", &m_pauseDiurnal);
        // Longitude sliderfloat
        ImGui::SliderFloat("Longitude", &m_longitude, 0, 90);
        // Day setting (day/month)
        m_day = chooseDateDay();

        //--Extra settings--
        ImGui::SliderFloat("Sun Strength", &m_sunStrength, 0.0f, 10.0f);

        ImGui::TreePop();
    }
	if (ImGui::TreeNode("Environment"))
	{

		ImGui::Text(std::string("Currently Editing: " + std::to_string(m_editParamSky)).c_str());
		ImGui::Text("Edit parameter of:");
		if (ImGui::Button("Temp")) m_editParamSky = POTTEMP;		ImGui::SameLine();
		if (ImGui::Button("Wind")) m_editParamSky = WINDX;		ImGui::SameLine();
		if (ImGui::Button("Qv")) m_editParamSky = QV;		ImGui::SameLine();
		if (ImGui::Button("Qw")) m_editParamSky = QW;
		if (ImGui::Button("Qc")) m_editParamSky = QC;		ImGui::SameLine();
		if (ImGui::Button("Qr")) m_editParamSky = QR;		ImGui::SameLine();
		if (ImGui::Button("Qs")) m_editParamSky = QS;		ImGui::SameLine();
		if (ImGui::Button("Qi")) m_editParamSky = QI;
		if (ImGui::Button("Ground")) m_editParamSky = PGROUND;

		static int e = 0;
		ImGui::RadioButton("Empty", &e, 0); ImGui::SameLine();
		ImGui::RadioButton("Brush", &e, 1); ImGui::SameLine();
		ImGui::RadioButton("Select", &e, 2);

		if (e == 0) // Empty
		{
			m_brushing = false;
            // Depends if we are still selecting with microphysics selection
			m_selecting = m_microPhysDataSelect; 
            
		}
		else if (e == 1) // Brushing
		{
			m_brushing = true;
			m_selecting = false;
			m_microPhysDataSelect = false;

			glm::vec2 minMax = getMinMaxVaueParam(m_editParamSky);
			ImGuiSliderFlags sliderFlag = 0;
			const char* format = getFormatParam(m_editParamSky, sliderFlag);

			ImGui::SliderFloat("Radius", &m_brushSize, 0.5f, 32.0f, "%.3f");
			ImGui::SliderFloat("Smoothness", &m_brushSmoothness, 0.01f, 10.0f, "%.1f");
			ImGui::Text("Value that will be applied every second:");
			ImGui::SliderFloat("AppliedValue", &m_applyValue, minMax.x, minMax.y, format, sliderFlag);
			ImGui::SliderFloat("Intensity", &m_brushIntensity, -1.0f, 1.0f, "%.7f", ImGuiSliderFlags_Logarithmic);

			//Vec3
            if (m_editParamSky == WINDX || m_editParamSky == WINDY || m_editParamSky == WINDZ)
			{
				vectorArrow();
			}
			else if (m_editParamSky == PGROUND)
			{
				ImGui::Checkbox("erase", &m_groundErase);
			}
		}
		else if (e == 2) // Selecting
		{
			m_brushing = false;
			m_selecting = true;
			m_microPhysDataSelect = false;

			glm::vec2 minMax = getMinMaxVaueParam(m_editParamSky);
			ImGuiSliderFlags sliderFlag = 0;
			const char* format = getFormatParam(m_editParamSky, sliderFlag);

			ImGui::SliderFloat("Value", &m_applyValue, minMax.x, minMax.y, format, sliderFlag);

			//Vec2
			if (m_editParamSky == 7)
			{
				vectorArrow();
			}
		}
		ImGui::TreePop();
	}
}


void simulationEditor::skewTTexturePanel()
{
    // Actually draw the skewT from the current selected index onto ImGui texture

    glm::vec2 first{-85, -5};
    glm::vec2 second{5, 100};
    static unsigned int skewTTexture = 0;

    m_skewTObj->getSkewTImage()->getDrawnImage(first, second, skewTTexture);

    // Display texture in ImGui
    ImVec2 max = ImGui::GetContentRegionMax();
    max.x = std::min(max.x - 16.0f, max.y - 50.0f);
    max.y = max.x;
    ImTextureID ITID = static_cast<ImTextureID>(skewTTexture);
    ImGui::Image(ITID, max, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
}

void simulationEditor::setSkewTDataPanel()
{
    // Settings like zooming in and skewing
    if (ImGui::TreeNode("SkewTSettings"))
    {
        ImGui::SliderFloat2("Pos", &m_skewTObj->skewTPos.x, -360, 360);
        if (ImGui::SliderFloat("Size", &m_skewTObj->skewTSize.x, 1, 1000))
        {
            m_skewTObj->skewTSize.y = m_skewTObj->skewTSize.x;
        }
        if (ImGui::SliderFloat("Skew", &m_skewTObj->tanTheta, 0, 89, "%.0f"))
        {
            m_skewTObj->tanTheta = glm::tan(glm::radians(m_skewTObj->tanTheta));
        }
        ImGui::TreePop();
    }
}

void simulationEditor::dataViewPanel(bool simActive) 
{
    if (m_dataClassObj->microPhysCheckActive)
    {
        CloudHub.CloudSim().Environment().retrieveMicroPhysResults(*m_dataClassObj);
    }
    m_dataClassObj->drawMicroPhysGraph(simActive);

    if (ImGui::Button("Reset"))
    {
        m_dataClassObj->cancelMicroPhysCheckRegion();
        CloudHub.CloudSim().Environment().setMicroPhysDataValues({-1, -1, -1}, {-1, -1, -1}, false);
    }
}

void simulationEditor::dataSettingsPanel()
{
    ImGui::SeparatorText("MicroPhysics Graph");
    if (ImGui::Button("Select Region"))
    {
        m_microPhysDataSelect = true;
        m_selecting = true;
        m_justViewSelection = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
    {
        m_microPhysDataSelect = false;
        m_selecting = false;
        m_justViewSelection = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("View Region"))
    {
        m_justViewSelection = true;
        m_selecting = false;
        m_microPhysDataSelect = false;
    }
}

void simulationEditor::shortCutViewDataPanel() 
{
    if (CloudHub.InputObj().keyDown(newInput::SPACEBAR))
    {
        viewTooltipDataPanel();
    }
    if (CloudHub.InputObj().keyDown(newInput::LEFT_ALT))
    {
        viewPickerPanel();
    }
}

void simulationEditor::viewTooltipDataPanel() 
{
    int x = 0, y = 0, z = 0;
    getCoord(m_mousePointingIndex, x, y, z);
    const int idx = m_mousePointingIndex;

    const float height = y * VOXELSIZE;
    const float Tz = float(m_envEditData->m_envView.potTemp[idx]) - 273.15f;
    const float T = MForms::potentialTemp(Tz, m_envEditData->m_groundView.P[int(x)], m_envEditData->m_envView.pressure[idx]);

    const float rs = MForms::ws(T, m_envEditData->m_envView.pressure[idx]);
    const float RH = m_envEditData->m_envView.Qv[m_mousePointingIndex] / rs * 100;
    // Dew point calculation https://www.omnicalculator.com/physics/dew-point
    float dew = 0.0f;
    {
        const float a = 17.625f;
        const float b = 243.04f;
        const float c = log(RH / 100) + a * T / (b + T);
        dew = (b * c) / (a - c);
    }
    ImGui::SetTooltip("T   %.4fC,	 D %.4fC \nRH %.4f%%,	 h %.1fm", T, dew, RH, height);
}

void simulationEditor::viewPickerPanel()
{
    // Pick values to set as applying value
    int flag = 0;
	const char* format = getFormatParam(m_viewParamSky, flag);
	std::string pickerString = "Value: " + std::string(format);

	if (m_viewParamSky == WINDX || m_viewParamSky == WINDY || m_viewParamSky == WINDZ)
	{
		pickerString = pickerString + ", " + std::string(format) + ", " + std::string(format);
		ImGui::SetTooltip(pickerString.c_str(), m_applyValue * m_valueDir.x, m_applyValue * m_valueDir.y, m_applyValue * m_valueDir.z);
	}
	else
	{
		ImGui::SetTooltip(pickerString.c_str(), m_applyValue);
	}
}

int simulationEditor::chooseDateDay()
{
    // Menu to select day of the year
    static int defaultDay = m_day;
	int currentMonth = 6;
	int currentDay = 1;
	dayToMonthDay(m_day, currentMonth, currentDay);

	ImGui::Text("Current Day: %i-%i", currentMonth + 1, currentDay);
	ImVec2 buttonSize = { ImGui::CalcTextSize(" Choose Day ").x + 10, 40 };

	if (ImGui::Button("Choose Day", buttonSize))
	{
		ImGui::OpenPopup("StartDate");
	}
	if (ImGui::BeginPopup("StartDate"))
	{

		static std::string Months[12] = {
		"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"
		};

		ImVec2 windowSize = ImGui::GetWindowSize();
		float textSize = ImGui::GetFontSize();
		float currentTextSize = 0.0f;
		ImVec2 largestMonthSize = ImGui::CalcTextSize(Months[8].c_str());

		//Now using defaultDay to show edited day
		dayToMonthDay(defaultDay, currentMonth, currentDay);

		//Month
		{
			ImGui::SetCursorPosX(windowSize.x * 0.5f - largestMonthSize.x * 0.5f - textSize);
			if (ImGui::Button("<##MonthBack") && currentMonth > 0)
			{
				currentMonth--;
			}
			ImGui::SameLine();
			currentTextSize = ImGui::CalcTextSize(Months[currentMonth].c_str()).x;
			ImGui::SetCursorPosX(windowSize.x * 0.5f - currentTextSize * 0.5f);
			ImGui::Text("%s", Months[currentMonth].c_str());
			ImGui::SameLine();

			ImGui::SetCursorPosX(windowSize.x * 0.5f + largestMonthSize.x * 0.5f + textSize);
			if (ImGui::Button(">##MonthForward") && currentMonth < 11)
			{
				currentMonth++;
			}
		}

		//Day
		{
			buttonSize = { 40,40 };
			int daysInMonth = getDaysInMonth(currentMonth);

			for (int i = 1; i < daysInMonth + 1; i++)
			{
				char label[8];
				std::snprintf(label, sizeof(label), "%i", i);
				if (ImGui::Button(label, buttonSize))
				{
					currentDay = i;
				}
				if ((i % 7 != 0 || i == 0) && i != daysInMonth)
				{
					ImGui::SameLine();
				}
			}
		}

		ImGui::Text("Selected Start Date: %i-%i", currentMonth + 1, currentDay);

		ImGui::SameLine();

		//Confirm
		int confirmDay = m_day;

		defaultDay = 0;
		for (int i = 0; i < currentMonth; i++)
		{
			for (int j = 0; j < getDaysInMonth(i); j++)
			{
				defaultDay++;
			}
		}
		defaultDay += currentDay;

		if (ImGui::Button("Ok"))
		{
			confirmDay = defaultDay;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
		return confirmDay;
	}
	return m_day;
}

int simulationEditor::getDaysInMonth(int month)
{
    int output = month % 2 ? 30 : 31;  // 0 = January.
    return month == 1 ? 28 : output;
}

void simulationEditor::dayToMonthDay(int dayOfYear, int& month, int& dayOfMonth)
{
    int count = 0;
    for (int i = 0; i < 12; i++)
    {
        dayOfMonth = dayOfYear - count;
        count += getDaysInMonth(i);
        if (count > dayOfYear - 1)
        {
            month = i;
            return;
        }
    }
}

glm::vec2 simulationEditor::getMinMaxVaueParam(parameter param) 
{ 
    // Return self-set value of parameter
    switch (param)
    {
        case POTTEMP:
            return {0, 373.15f};
            break;
        case WINDX: case WINDY: case WINDZ:
            return {-50.0f, 50.0f};
            break;
        case PGROUND:
            break;
        default:
            // All mixing rations
            if (static_cast<int>(param) > 0 && static_cast<int>(param) < 7)
            {
                return {0.00001, 1.0f};
            }
            break;
    }

    return {0, 1.0f};
}

const char* simulationEditor::getFormatParam(parameter param, int& flagOutput) 
{ 
    flagOutput = 0;

    // Return self-set format of parameter
    switch (param)
    {
        case POTTEMP:
            return "%.2f";
            break;
        case WINDX: case WINDY: case WINDZ:
            return "%.3f";
            break;
        case PGROUND:
            break;
        default:
            // All mixing rations
            if (static_cast<int>(param) > 0 && static_cast<int>(param) < 7)
            {
                flagOutput = static_cast<int>(ImGuiSliderFlags_Logarithmic);
                return "%.7f";
            }
            // All Debug values
            else if (static_cast<int>(param) > 8 && static_cast<int>(param) < 12)
            {
                return "%.6f";
            }
            break;
    }
    return "%.1f";
}

void simulationEditor::vectorArrow() 
{
    // Draw vector arrow using ImGuizmo to indicate rotation
    // Using help from Claude AI
    static float hor = 0.0f;  // horizontal angle degrees
    static float ver = 0.0f;  // vertical angle degrees

    // Set vertical and horizontal values
    ImGui::SliderFloat("sides", &hor, -180.0f, 180.0f, "%.1f deg");
    ImGui::SameLine();
    if (ImGui::Button("X##hor")) hor = 0.0f;
    ImGui::SliderFloat("up", &ver, -90.0f, 90.0f, "%.1f deg");
    ImGui::SameLine();
    if (ImGui::Button("X##ver")) ver = 0.0f;

    // Convert to 3D direction
    float h = glm::radians(hor);
    float v = glm::radians(ver);
    glm::vec3 direction = glm::vec3(cosf(v) * sinf(h), sinf(v), cosf(v) * cosf(h));
    // Make matrix out of it using lookat function
    glm::mat4 viewMat = glm::lookAt(-direction, glm::vec3(0), glm::vec3(0, 1, 0));
    float view[16];
    memcpy(view, glm::value_ptr(viewMat), 16 * sizeof(float));

    ImVec2 gizmoPos = ImGui::GetCursorScreenPos();
    ImVec2 gizmoSize = ImVec2(150, 150);

    // Get right, up and forward from matrix
    glm::mat4 invView = glm::inverse(viewMat);
    glm::vec3 rightDir = glm::vec3(invView[0]);    // X axis — red
    glm::vec3 upDir = glm::vec3(invView[1]);       // Y axis — green
    glm::vec3 forwardDir = glm::vec3(invView[2]);  // Z axis — blue
    ImVec2 center = ImVec2(gizmoPos.x + gizmoSize.x * 0.5f, gizmoPos.y + gizmoSize.y * 0.5f);
    float axisLen = 40.0f;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Project direction into positions using axisLen as offset
    ImVec2 tipX = ImVec2(center.x + rightDir.x * axisLen, center.y - rightDir.y * axisLen);
    ImVec2 tipY = ImVec2(center.x + upDir.x * axisLen, center.y - upDir.y * axisLen);
    ImVec2 tipZ = ImVec2(center.x + forwardDir.x * axisLen, center.y - forwardDir.y * axisLen);
    ImVec2 tipArrow = ImVec2(center.x + direction.x * axisLen, center.y - direction.y * axisLen);

    // Ready to draw
    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
    ImGuizmo::SetRect(gizmoPos.x, gizmoPos.y, gizmoSize.x, gizmoSize.y);
    ImGui::Dummy(gizmoSize);

    // Arrow line, indicating direction
    dl->AddLine(center, tipArrow, IM_COL32(255, 255, 255, 255), 2.0f);  // X white
    // Circle on tip
    dl->AddCircleFilled(tipArrow, 4.0f, IM_COL32(255, 255, 255, 255));

    // Helping visualisers
    dl->AddLine(center, tipX, IM_COL32(255, 60, 60, 255), 1.0f);  // X red
    dl->AddLine(center, tipY, IM_COL32(60, 255, 60, 255), 1.0f);  // Y green
    dl->AddLine(center, tipZ, IM_COL32(60, 60, 255, 255), 1.0f);  // Z blue

    ImGui::Text("Direction: %.2f %.2f %.2f", direction.x, direction.y, direction.z);

    // Set data
    m_valueDir = direction;
}

void simulationEditor::GPUSetEnv(bool setIsentropics) 
{
    // Already locks itself
    CloudHub.CloudSim().Environment().setHostData(*m_envEditData, setIsentropics);

    lockGlobal();

    float passedTime = m_timeChanged ? m_time : -1.0f;
    CloudHub.CloudSim().Environment().setHostSettingsData(passedTime, m_day, m_sunStrength, m_longitude, m_pauseDiurnal);
    m_time = passedTime;
    m_timeChanged = false;

    unlockGlobal();

    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess)
    {
        std::cerr << "error: " << cudaGetErrorString(err) << std::endl;
        __debugbreak();
    }

}

void simulationEditor::viewBrush() 
{
    if (m_brushing)
    {
        int x = m_mousePointingPos.x;
        int y = m_mousePointingPos.y;
        int z = m_mousePointingPos.z;

        const int brushSizeI = int(ceil(m_brushSize));
        const float maxBrushSize = m_brushSize * m_brushSize;
        for (int zb = std::max(0, z - brushSizeI); zb < std::min(GRIDSIZESKYZ, z + brushSizeI); zb++)
        {
            for (int yb = std::max(0, y - brushSizeI); yb < std::min(GRIDSIZESKYY, y + brushSizeI); yb++)
            {
                for (int xb = std::max(0, x - brushSizeI); xb < std::min(GRIDSIZESKYX, x + brushSizeI); xb++)
                {
                    const int dx = xb - x;
                    const int dy = yb - y;
                    const int dz = zb - z;

                    const int distance = (dx * dx) + (dy * dy) + (dz * dz);

                    if (distance > maxBrushSize) continue;

                    CloudHub.CloudRender().shapesGLObj().AddVoxel((glm::vec3(xb, yb, zb) + 0.5f) * VOXELSIZE, VOXELSIZE, Colors::WhiteA);
                }
            }
        }
    }

}

void simulationEditor::applyBrush()
{
    if (!CloudHub.InputObj().keyDown(newInput::LEFT_CONTROL) && m_brushing &&
        m_mouseWheel != CloudHub.InputObj().mouseScroll() && !CloudHub.cloudMenuObj().getIsPanelSelected())
    {
        const float diff = CloudHub.InputObj().mouseScroll() - m_mouseWheel;
        m_brushSize = m_brushSize + diff <= 0.0f ? m_brushSize : m_brushSize + diff;
    }

    if (m_brushing && CloudHub.InputObj().mouseDown(newInput::MOUSE_LEFT) && !CloudHub.cloudMenuObj().getIsPanelSelected())
    {
        lockGlobal();
        // Let environment handle brush logic on the GPU
        CloudHub.CloudSim().Environment().prepareBrushGPU(m_editParamSky,
                                                             m_brushSize,
                                                             {m_mousePointingPos.x, m_mousePointingPos.y, m_mousePointingPos.z},
                                                             m_brushSmoothness,
                                                             m_deltatime,
                                                             m_brushIntensity,
                                                             m_applyValue,
                                                             {m_valueDir.x, m_valueDir.y, m_valueDir.z},
                                                             m_groundErase);
        unlockGlobal();
    }
}

void simulationEditor::viewSelect() 
{
    int x = m_mousePointingPos.x;
    int y = m_mousePointingPos.y;
    int z = m_mousePointingPos.z;

    // Set correct selection posses
    if ((m_selecting) && !CloudHub.InputObj().keyDown(newInput::LEFT_SHIFT) &&
        !CloudHub.cloudMenuObj().getIsPanelSelected())
    {
        if (CloudHub.InputObj().mouseOnce(newInput::MOUSE_LEFT))
        {
            m_saveSelectPos = {x, y, z};
            m_selectReset = true;
        }
        else if (CloudHub.InputObj().mouseDown(newInput::MOUSE_LEFT))
        {
            if (m_saveSelectPos == glm::ivec3(0) && m_selectReset)
            {
                // Impossible (too precise), meaning went from tab to playfield
                m_saveSelectPos = {x, y, z};
            }

            m_corners[0].x = m_saveSelectPos.x;
            m_corners[0].y = m_saveSelectPos.y;
            m_corners[0].z = m_saveSelectPos.z;

            m_corners[1].x = x;
            m_corners[1].y = y;
            m_corners[1].z = z;

            m_selectReset = false;
        }
        else if (m_selectReset)
        {
            m_saveSelectPos = glm::ivec3(0);
        }
    }
    else if (CloudHub.cloudMenuObj().getIsPanelSelected())
    {
        m_saveSelectPos = glm::ivec3(0);
        m_selectReset = true;
    }

    // Render selection
    if ((m_selecting || m_justViewSelection) && glm::ivec3(x, y, z) != m_saveSelectPos)
    {
        glm::vec4 color = Colors::DodgerBlueA;
        if (m_justViewSelection) color = Colors::BlueA;

        int sx1 = std::max(0, std::min(m_corners[0].x, m_corners[1].x));
        int sy1 = std::max(0, std::min(m_corners[0].y, m_corners[1].y));
        int sz1 = std::max(0, std::min(m_corners[0].z, m_corners[1].z));
        int sx2 = std::min(GRIDSIZESKYX - 1, std::max(m_corners[1].x, m_corners[0].x));
        int sy2 = std::min(GRIDSIZESKYY - 1, std::max(m_corners[1].y, m_corners[0].y));
        int sz2 = std::min(GRIDSIZESKYZ - 1, std::max(m_corners[1].z, m_corners[0].z));

        for (int sz = sz1; sz <= sz2; sz++)
        {
            for (int sy = sy1; sy <= sy2; sy++)
            {
                for (int sx = sx1; sx <= sx2; sx++)
                {
                    CloudHub.CloudRender().shapesGLObj().AddVoxel((glm::vec3(sx, sy, sz) + 0.5f) * VOXELSIZE, VOXELSIZE, color);
                }
            }
        }
    }

}

void simulationEditor::applySelect()
{
    if ((m_selecting) && m_saveSelectPos != glm::ivec3(0) && !CloudHub.InputObj().mouseDown(newInput::MOUSE_LEFT))
    {
        int minX = int(std::min(m_corners[0].x, m_corners[1].x));
        int minY = int(std::min(m_corners[0].y, m_corners[1].y));
        int minZ = int(std::min(m_corners[0].z, m_corners[1].z));
        int maxX = int(std::max(m_corners[0].x, m_corners[1].x));
        int maxY = int(std::max(m_corners[0].y, m_corners[1].y));
        int maxZ = int(std::max(m_corners[0].z, m_corners[1].z));

        lockGlobal();
        if (m_microPhysDataSelect)
        {
            // Activate microphysics data collection inside environment.cu
            m_dataClassObj->confirmMicroPhysCheckRegion({minX, minY, minZ}, {maxX, maxY, maxZ});
            CloudHub.CloudSim().Environment().setMicroPhysDataValues({minX, minY, minZ}, {maxX, maxY, maxZ}, true);
            m_selecting = false;
            m_microPhysDataSelect = false;
            m_justViewSelection = true;
            CloudHub.CameraObj().enable(); // We have to manually enable camera
        }
        else
        {
            // Brush
            CloudHub.CloudSim().Environment().prepareSelectionGPU(m_editParamSky,
                                                                     {minX, minY, minZ},
                                                                     {maxX, maxY, maxZ},
                                                                     m_applyValue,
                                                                     {m_valueDir.x, m_valueDir.y, m_valueDir.z},
                                                                     m_groundErase);
            m_selecting = false;
            m_justViewSelection = true;
        }
        unlockGlobal();
    }
}

void simulationEditor::usePicker() 
{
    if (CloudHub.InputObj().keyDown(newInput::LEFT_ALT))
    {
        if (!isOutside(m_mousePointingPos.x, m_mousePointingPos.y, m_mousePointingPos.z))
        {
            if (m_viewParamSky == WINDX || m_viewParamSky == WINDY || m_viewParamSky == WINDZ)
            {
                // Using normalize and dividing will result in exact value if trying to set value
                glm::vec3 value;
                value.x = m_applyValue = getValueParam(m_mousePointingIndex, WINDX);
                value.x = m_applyValue = getValueParam(m_mousePointingIndex, WINDY);
                value.x = m_applyValue = getValueParam(m_mousePointingIndex, WINDZ);

                m_valueDir = glm::normalize(value);
                m_applyValue = value.x / m_valueDir.x;
            }
            else
            {
                m_applyValue = getValueParam(m_mousePointingIndex, m_viewParamSky);
            }
        }
    }
}

void simulationEditor::viewDebugSky() 
{
    if (!m_viewSky) return;

	// Usage of min and max view to possibly use slices
	for (int z = m_minViewZ; z < m_maxViewZ; z++)
	{
		for (int y = m_minViewY; y < m_maxViewY; y++)
		{
			for (int x = m_minViewX; x < m_maxViewX; x++)
			{
				const int idx = getIdx(x, y, z);
				const int idxG = x + z * GRIDSIZESKYX;
				if (y <= m_envEditData->m_groundHeight[idxG]) 
				{
					continue; // Ignore
				}

				glm::vec3 color{};
				switch (m_viewParamSky)
				{
				case POTTEMP:
				{
					//Get temp
					const float Tz = float(m_envEditData->m_envView.potTemp[idx]) - 273.15f;
					const float T = MForms::potentialTemp(Tz, m_envEditData->m_groundView.P[idxG], m_envEditData->m_envView.pressure[idx]) + 273.15f;

					m_colorSchemeObj->getColor("TemperatureSky", T, color);
					break;
				}
				case QV:
					m_colorSchemeObj->getColor("mixingRatio", m_envEditData->m_envView.Qv[idx], color);
					break;
				case QW:
					m_colorSchemeObj->getColor("mixingRatio", m_envEditData->m_envView.Qw[idx], color);
					break;
				case QC:
					m_colorSchemeObj->getColor("mixingRatio", m_envEditData->m_envView.Qc[idx], color);
					break;
				case QR:
					m_colorSchemeObj->getColor("mixingRatio", m_envEditData->m_envView.Qr[idx], color);
					break;
				case QS:
					m_colorSchemeObj->getColor("mixingRatio", m_envEditData->m_envView.Qs[idx], color);
					break;
				case QI:
					m_colorSchemeObj->getColor("mixingRatio", m_envEditData->m_envView.Qi[idx], color);
					break;
				case WINDX:
				case WINDY:
				case WINDZ:
					m_colorSchemeObj->getColor("velField", glm::length(glm::vec3(m_envEditData->m_envView.velFieldX[idx], m_envEditData->m_envView.velFieldY[idx], m_envEditData->m_envView.velFieldZ[idx])), color);
					//bee::Engine.DebugRenderer().AddArrow(bee::DebugCategory::All, glm::vec3(x + 0.5f, y + 0.5f, z + 0.5f), glm::vec3(0.0f, 0.0f, 1.0f), VelUV, 0.9f, bee::Colors::Black);
					break;
				case PRESSURE:
					m_colorSchemeObj->getColor("pressure", m_envEditData->m_envView.pressure[idx], color);
					break;
				case DEBUG1:
				{
					//Currently for realistic view
					const float allValues = m_envEditData->m_envView.Qw[idx] + m_envEditData->m_envView.Qc[idx] +
						m_envEditData->m_envView.Qr[idx] + m_envEditData->m_envView.Qs[idx] + m_envEditData->m_envView.Qi[idx];

					m_colorSchemeObj->getColor("realistic", allValues, color);
					if (allValues < 0.0001) continue;
					break;
				}
				case DEBUG2:
					m_colorSchemeObj->getColor("debugColor", m_envEditData->m_debugArray1[idx], color);
					break;
				case DEBUG3:
					m_colorSchemeObj->getColor("debugColor", m_envEditData->m_debugArray2[idx], color);
					break;
				default:
					break;
				}
				
				m_tracingObj->setVoxelValue(idx, true); // Add voxel to tracer so we can select it

                if (m_wireFrame) CloudHub.CloudRender().shapesGLObj().AddVoxel((glm::vec3(x, y, z) + 0.5f) * VOXELSIZE, 0.999f * VOXELSIZE, { color, 1.0f });
				else CloudHub.CloudRender().shapesGLObj().AddFilledVoxel((glm::vec3(x, y, z) + 0.5f) * VOXELSIZE, 0.999f * VOXELSIZE, { color, 1.0f });
			}
		}
	}
}

void simulationEditor::viewDebugGround() 
{
    if (!m_viewGround) return;

    for (int z = 0; z < GRIDSIZESKYZ; z++)
    {
        for (int x = 0; x < GRIDSIZESKYX; x++)
        {
            const int idxG = x + z * GRIDSIZESKYX;
            const int GHeight = m_envEditData->m_groundHeight[x + z * GRIDSIZESKYX];
            glm::vec3 color{};
            switch (m_viewParamGround)
            {
                case 0:
                    m_colorSchemeObj->getColor("TemperatureSky", float(m_envEditData->m_groundView.T[idxG]), color);
                    break;
                case 1:
                    m_colorSchemeObj->getColor("mixingRatio", m_envEditData->m_groundView.Qrs[idxG], color);
                    break;
                case 2:
                    m_colorSchemeObj->getColor("mixingRatio", m_envEditData->m_groundView.Qgr[idxG], color);
                    break;
                case 3:
                    m_colorSchemeObj->getColor("mixingRatio", m_envEditData->m_groundView.Qgs[idxG], color);
                    break;
                case 4:
                    m_colorSchemeObj->getColor("mixingRatio", m_envEditData->m_groundView.Qgi[idxG], color);
                    break;
            }

            if (m_wireFrame) CloudHub.CloudRender().shapesGLObj().AddVoxel((glm::vec3(x, GHeight, z) + 0.5f) * VOXELSIZE, 0.999f * VOXELSIZE, {color, 1.0f});
            else CloudHub.CloudRender().shapesGLObj().AddFilledVoxel((glm::vec3(x, GHeight, z) + 0.5f) * VOXELSIZE, 0.999f * VOXELSIZE, {color, 1.0f});

            m_tracingObj->setVoxelValue(getIdx(x, GHeight, z), true);  // Add voxel to tracer so we can select it
        }
    }

}

float simulationEditor::getValueParam(const int i, parameter param) 
{ 
    switch (param)
    {
        case POTTEMP: return {m_envEditData->m_envView.potTemp[i]}; break;
        case QV: return {m_envEditData->m_envView.Qv[i]}; break;
        case QW: return {m_envEditData->m_envView.Qw[i]}; break;
        case QC: return {m_envEditData->m_envView.Qc[i]}; break;
        case QR: return {m_envEditData->m_envView.Qr[i]}; break;
        case QS: return {m_envEditData->m_envView.Qs[i]}; break;
        case QI: return {m_envEditData->m_envView.Qi[i]}; break;
        case WINDX: return {m_envEditData->m_envView.velFieldX[i]}; break;
        case WINDY: return {m_envEditData->m_envView.velFieldY[i]}; break;
        case WINDZ: return {m_envEditData->m_envView.velFieldZ[i]}; break;
        case PGROUND: return {0}; break;
        case DEBUG1: return {m_envEditData->m_debugArray0[i]}; break;
        case DEBUG2: return {m_envEditData->m_debugArray1[i]}; break;
        case DEBUG3: return {m_envEditData->m_debugArray2[i]}; break;
        default:
            break;
    }
    return {0};
}

void simulationEditor::setSliceMinMax(bool fullView)
{
    // If going to full view, we reset
    if (fullView)
    {
        m_minViewX = 0;
        m_minViewY = 0;
        m_minViewZ = 0;
        m_maxViewX = GRIDSIZESKYX;
        m_maxViewY = GRIDSIZESKYY;
        m_maxViewZ = GRIDSIZESKYZ;
    }
    else
    {
        // Based on the coordinate, we set our min and max to limit the current coord.
        switch (m_viewSliceCoord)
        {
            case 0:  // Slice X
                m_atSliceViewSlice = std::min(m_atSliceViewSlice, GRIDSIZESKYX - 1);
                m_minViewX = m_atSliceViewSlice;
                m_maxViewX = m_atSliceViewSlice + 1;

                m_minViewY = 0;
                m_minViewZ = 0;
                m_maxViewY = GRIDSIZESKYY;
                m_maxViewZ = GRIDSIZESKYZ;
                break;
            case 1:  // Slice Y
                m_atSliceViewSlice = std::min(m_atSliceViewSlice, GRIDSIZESKYY - 1);
                m_minViewY = m_atSliceViewSlice;
                m_maxViewY = m_atSliceViewSlice + 1;

                m_minViewX = 0;
                m_minViewZ = 0;
                m_maxViewX = GRIDSIZESKYX;
                m_maxViewZ = GRIDSIZESKYZ;
                break;
            case 2:  // Slice Z
                m_atSliceViewSlice = std::min(m_atSliceViewSlice, GRIDSIZESKYZ - 1);
                m_minViewZ = m_atSliceViewSlice;
                m_maxViewZ = m_atSliceViewSlice + 1;

                m_minViewX = 0;
                m_minViewY = 0;
                m_maxViewX = GRIDSIZESKYX;
                m_maxViewY = GRIDSIZESKYY;
                break;
            default:
                break;
        }
    }
}

