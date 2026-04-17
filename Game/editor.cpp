#include "environment.h"
#include "editor.h"
#include "skewTer.h"
#include "dataClass.cuh"
//#include "game.h" //Done in .h due to need for USE_GPU

#include "math/meteoformulas.h"
#include "math/constants.hpp"
#include "math/geometry.hpp"

#include "core/engine.hpp"
#include "core/input.hpp"
#include "core/transform.hpp"

#include "rendering/colors.hpp"
#include "rendering/debug_render.hpp"
#include "rendering/render.hpp"

#include "platform/opengl/render_gl.hpp"
#include "platform/opengl/draw_image.hpp"

#include "tools/inspector.hpp"

#include "imgui/IconsFontAwesome.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "utils.cuh"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#if USE_GPU
#include "environment.cuh"
#include "kernelMaths.cuh"
#include "kernelSky.cuh"
#include <cuda.h>
#include <cuda_runtime.h>
#endif


editor::editor(envDebugData* _envDebugData)
{
	m_envData = _envDebugData;
	m_backGroundColor = { 0.35f, 0.55f, 0.9f };
}

editor::~editor()
{
	//Cleanup
	delete m_envData; //Created from environment.cpp
}

void editor::setColors()
{
	//ColorSchemes
	auto& colorScheme = bee::Engine.DebugRenderer().GetColorScheme();
	colorScheme.createColorScheme("TemperatureSky", -40 + 273.15f, bee::Colors::Blue, 40 + 273.15f, bee::Colors::Red);
	colorScheme.addColor("TemperatureSky", -10 + 273.15f, bee::Colors::Cyan);
	colorScheme.addColor("TemperatureSky", 10 + 273.15f, bee::Colors::Green);
	colorScheme.addColor("TemperatureSky", 20 + 273.15f, bee::Colors::Yellow);

	colorScheme.createColorScheme("mixingRatio", 0.0f, bee::Colors::Blue, 1.0f, bee::Colors::Red);
	colorScheme.addColor("mixingRatio", 0.0005f, bee::Colors::Cyan);
	colorScheme.addColor("mixingRatio", 0.001f, bee::Colors::Green);
	colorScheme.addColor("mixingRatio", 0.01f, bee::Colors::Yellow);

	colorScheme.createColorScheme("velField", 0, bee::Colors::Blue, 100, bee::Colors::Red);
	colorScheme.addColor("velField", 10, bee::Colors::Cyan);
	colorScheme.addColor("velField", 25, bee::Colors::Green);
	colorScheme.addColor("velField", 50, bee::Colors::Yellow);

	colorScheme.createColorScheme("debugColor", -1, bee::Colors::Purple * 0.2f, 1, bee::Colors::White);
	colorScheme.addColor("debugColor", -0.01f, bee::Colors::Purple);
	colorScheme.addColor("debugColor", -0.01f, bee::Colors::Blue);
	colorScheme.addColor("debugColor", -0.001f, bee::Colors::DodgerBlue);
	colorScheme.addColor("debugColor", -0.0001f, bee::Colors::Cyan);
	colorScheme.addColor("debugColor", 0.0f, bee::Colors::Green);
	colorScheme.addColor("debugColor", 0.0001f, bee::Colors::Yellow);
	colorScheme.addColor("debugColor", 0.001f, bee::Colors::Orange);
	colorScheme.addColor("debugColor", 0.01f, bee::Colors::Red);
	colorScheme.addColor("debugColor", 0.1f, bee::Colors::Pink + glm::vec4(0, 0.8f, 0, 0));

	colorScheme.createColorScheme("realistic", 0.0f, bee::Colors::Blue, 1.0f, bee::Colors::Black);
	colorScheme.addColor("realistic", 0.0005f, glm::vec4(0.6f, 0.9f, 1.0f, 1.0f));
	colorScheme.addColor("realistic", 0.001f, bee::Colors::White);
	colorScheme.addColor("realistic", 0.005f, glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
	colorScheme.addColor("realistic", 0.01f, glm::vec4(0.4f, 0.4f, 0.4f, 1.0f));

	colorScheme.createColorScheme("pressure", 0.0f, bee::Colors::Purple, 1050.0f, bee::Colors::White);
	colorScheme.addColor("pressure", 100.0f, glm::vec4(0.3f, 0.0f, 0.75f, 1.0f));
	colorScheme.addColor("pressure", 200.0f, glm::vec4(0.1f, 0.8f, 0.9f, 1.0f));
	colorScheme.addColor("pressure", 300.0f, glm::vec4(0.0f, 0.8f, 1.0f, 1.0f));
	colorScheme.addColor("pressure", 400.0f, glm::vec4(0.0f, 0.4f, 1.0f, 1.0f));
	colorScheme.addColor("pressure", 600.0f, glm::vec4(0.0f, 0.8f, 0.8f, 1.0f));
	colorScheme.addColor("pressure", 700.0f, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
	colorScheme.addColor("pressure", 800.0f, glm::vec4(0.4f, 0.8f, 0.0f, 1.0f));
	colorScheme.addColor("pressure", 900.0f, glm::vec4(0.8f, 0.0f, 0.0f, 1.0f));
	colorScheme.addColor("pressure", 950.0f, glm::vec4(1.0f, 0.6f, 0.6f, 1.0f));
	colorScheme.addColor("pressure", 975.0f, glm::vec4(0.6f, 0.5f, 0.5f, 1.0f));
	colorScheme.addColor("pressure", 1000.0f, glm::vec4(0.2f, 0.2f, 0.2f, 1.0f));

	colorScheme.createColorScheme("density", -1, bee::Colors::Purple * 0.0f, 1, bee::Colors::White);
	colorScheme.addColor("density", 0.15f, bee::Colors::Purple);
	colorScheme.addColor("density", 0.3f, bee::Colors::Blue);
	colorScheme.addColor("density", 0.5f, bee::Colors::DodgerBlue);
	colorScheme.addColor("density", 0.75f, bee::Colors::Cyan);
	colorScheme.addColor("density", 0.9f, bee::Colors::Green);
	colorScheme.addColor("density", 1.0f, bee::Colors::Yellow);
	colorScheme.addColor("density", 1.1f, bee::Colors::Orange);
	colorScheme.addColor("density", 1.2f, bee::Colors::Red);
	colorScheme.addColor("density", 1.225f, bee::Colors::Pink + glm::vec4(0, 0.8f, 0, 0));
}

void editor::setIsentropics(float* isentropicTemps, float* isentropicVapor, float* pressures)
{
	//Init skewTer
	{
		float* temps = new float[GRIDSIZESKYY];
		float* dewPoints = new float[GRIDSIZESKYY];
		float* ps = new float[GRIDSIZESKYY];

#if USE_GPU
		cudaMemcpy(ps, pressures, GRIDSIZESKYY * sizeof(float), cudaMemcpyDeviceToHost);
#else
		memcpy(ps, pressures, GRIDSIZESKYY * sizeof(float));
#endif

		dataToSkewTData(temps, dewPoints, ps);

		skewTer::skewTInfo skewT;
		skewT.init(GRIDSIZESKYY, temps, dewPoints, ps);
		m_skewTidx = (m_envData->m_groundHeight[0] + 1) * GRIDSIZESKYX;
		Game.SkewT().setSkewT(skewT);

		delete[] temps;
		delete[] dewPoints;
		delete[] ps;
	}

#if USE_GPU
	cudaMemcpy(m_envData->m_envTemp, isentropicTemps, GRIDSIZESKYY * sizeof(float), cudaMemcpyDeviceToHost);
	cudaMemcpy(m_envData->m_envVapor, isentropicVapor, GRIDSIZESKYY * sizeof(float), cudaMemcpyDeviceToHost);
	cudaMemcpy(m_envData->m_envPressure, pressures, GRIDSIZESKYY * sizeof(float), cudaMemcpyDeviceToHost);

#else
	memcpy(m_envData->m_envTemp, isentropicTemps, GRIDSIZESKYY * sizeof(float));
	memcpy(m_envData->m_envVapor, isentropicVapor, GRIDSIZESKYY * sizeof(float));
	memcpy(m_envData->m_envPressure, pressures, GRIDSIZESKYY * sizeof(float));
#endif
}

void editor::update(float dt)
{
	m_deltatime = dt;

	//Variable set
	setVariables();
	//Camera
	cameraControl();
	//Edit mode
	editMode();
}

void editor::panel()
{
	mainButtons();
	viewParamInformation();
	ImGui::Begin("Viewset");
	setView();
	setSlice();
	ImGui::End();
	editModeParams();
	setSkewTData();
	skewTTexture();
	dataClassView();
	viewMicroPhysGraph();
	viewImguiData();
}

void editor::viewData()
{
	viewBackground();
	viewSky();
	viewGround();
	viewSkewT();

	resetValues();
}

void editor::setDebugValueNum(const float* array, const int num)
{
#if USE_GPU
	switch (num)
	{
	case 0:
		cudaMemcpy(m_envData->m_debugArray0, array, GRIDSIZESKY * sizeof(float), cudaMemcpyDeviceToHost);
		break;
	case 1:
		cudaMemcpy(m_envData->m_debugArray1, array, GRIDSIZESKY * sizeof(float), cudaMemcpyDeviceToHost);
		break;
	case 2:
		cudaMemcpy(m_envData->m_debugArray2, array, GRIDSIZESKY * sizeof(float), cudaMemcpyDeviceToHost);
		break;
	default:
		break;
	}

#else
	switch (num)
	{
	case 0:
		memcpy(m_envData->m_debugArray0, array, GRIDSIZESKY * sizeof(float));
		break;
	case 1:
		memcpy(m_envData->m_debugArray1, array, GRIDSIZESKY * sizeof(float));
		break;
	case 2:
		memcpy(m_envData->m_debugArray2, array, GRIDSIZESKY * sizeof(float));
		break;
	default:
		break;
	}
#endif
}

void editor::GPUSetEnv(void* _sky, void* _ground, int* _groundHeight, float* _ps)
{
#if USE_GPU
	auto* sky = static_cast<environmentGPU::gridDataSkyGPU*>(_sky);
	auto* ground = static_cast<environmentGPU::gridDataGroundGPU*>(_ground);

	//Set sky values.
	cudaMemcpy(m_envData->m_envView.potTemp, sky->potTemp, GRIDSIZESKY * sizeof(float), cudaMemcpyDeviceToHost);
	cudaMemcpy(m_envData->m_envView.Qv, sky->Qv, GRIDSIZESKY * sizeof(float), cudaMemcpyDeviceToHost);
	cudaMemcpy(m_envData->m_envView.Qw, sky->Qw, GRIDSIZESKY * sizeof(float), cudaMemcpyDeviceToHost);
	cudaMemcpy(m_envData->m_envView.Qc, sky->Qc, GRIDSIZESKY * sizeof(float), cudaMemcpyDeviceToHost);
	cudaMemcpy(m_envData->m_envView.Qr, sky->Qr, GRIDSIZESKY * sizeof(float), cudaMemcpyDeviceToHost);
	cudaMemcpy(m_envData->m_envView.Qs, sky->Qs, GRIDSIZESKY * sizeof(float), cudaMemcpyDeviceToHost);
	cudaMemcpy(m_envData->m_envView.Qi, sky->Qi, GRIDSIZESKY * sizeof(float), cudaMemcpyDeviceToHost);
	
	static float velX[GRIDSIZESKY];
	static float velY[GRIDSIZESKY];
	static float velZ[GRIDSIZESKY];
	cudaMemcpy(velX, sky->velfieldX, GRIDSIZESKY * sizeof(float), cudaMemcpyDeviceToHost);
	cudaMemcpy(velY, sky->velfieldY, GRIDSIZESKY * sizeof(float), cudaMemcpyDeviceToHost);
	cudaMemcpy(velZ, sky->velfieldZ, GRIDSIZESKY * sizeof(float), cudaMemcpyDeviceToHost);
	for (int i = 0; i < GRIDSIZESKY; i++)
	{
		m_envData->m_envView.velField[i] = { velX[i], velY[i], velZ[i]};
	}
	//Set pressure
	cudaMemcpy(m_envData->m_envView.pressure, _ps, GRIDSIZESKY * sizeof(float), cudaMemcpyDeviceToHost);

	//Set Ground values	
	cudaMemcpy(m_envData->m_groundView.T, ground->T, GRIDSIZEGROUND * sizeof(float), cudaMemcpyDeviceToHost);
	cudaMemcpy(m_envData->m_groundView.Qrs, ground->Qrs, GRIDSIZEGROUND * sizeof(float), cudaMemcpyDeviceToHost);
	cudaMemcpy(m_envData->m_groundView.Qgr, ground->Qgr, GRIDSIZEGROUND * sizeof(float), cudaMemcpyDeviceToHost);
	cudaMemcpy(m_envData->m_groundView.Qgs, ground->Qgs, GRIDSIZEGROUND * sizeof(float), cudaMemcpyDeviceToHost);
	cudaMemcpy(m_envData->m_groundView.Qgi, ground->Qgi, GRIDSIZEGROUND * sizeof(float), cudaMemcpyDeviceToHost);
	cudaMemcpy(m_envData->m_groundView.P, ground->P, GRIDSIZEGROUND * sizeof(float), cudaMemcpyDeviceToHost);
	cudaMemcpy(m_envData->m_groundView.t, ground->t, GRIDSIZEGROUND * sizeof(float), cudaMemcpyDeviceToHost);
	cudaMemcpy(m_envData->m_groundHeight, _groundHeight, GRIDSIZEGROUND * sizeof(int), cudaMemcpyDeviceToHost);
#else
	_sky;
	_ground;
	_groundHeight;
	_ps;
#endif
}

void editor::editMode()
{
	m_changedGround = false;

	if (m_editMode || m_microPhysSelect)
	{
		if (m_editMode)
		{
			viewBrush();
			applyBrush();
			usePicker();
		}
		applySelect();
		viewSelection();
	}
	else if (m_justViewSelection)
	{
		viewSelection();
	}
}

void editor::cameraControl()
{
	//Reset state until we de-selected the inspector
//#ifdef BEE_INSPECTOR
	if (bee::Engine.Inspector().IsSelected())
	{
		SaveMousePos = MousePos3D;
		Save2DPos = bee::Engine.Input().GetMousePosition();
		glm::vec2 mousePos2D = bee::Engine.Input().GetMousePosition();
		MousePos3D = bee::screenToGround(bee::Engine.Input().GetMousePosition());
		return;
	}
//#endif

	//At cursor
	bee::Engine.DebugRenderer().AddCircle(bee::DebugCategory::General, MousePos3D, 0.5f, glm::vec3(0, 1, 0), glm::vec4(1.0));
	bee::Engine.DebugRenderer().AddFilledSquare(bee::DebugCategory::General, MousePos3D, 0.5f, glm::vec3(0, 1, 0), glm::vec4(1.0));

	//Keybinds = Left-Shift + mouse
	bool LeftShift = bee::Engine.Input().GetKeyboardKey(bee::Input::KeyboardKey::LeftShift);
	bool LeftCrtl = bee::Engine.Input().GetKeyboardKey(bee::Input::KeyboardKey::LeftControl);

	//If in view mode, just always be able to move.
	if ((!m_brushing && !m_selecting) || LeftShift)
	{


		//For each camera (we have 1)
		for (const auto& [entity, camera, transform] : bee::Engine.ECS().Registry.view<bee::Camera, bee::Transform>().each())
		{
			//------------------------------------------------------------------------------
			//--------------------------Moving around---------------------------------------
			//------------------------------------------------------------------------------

			float cameraSpeed = 25.0f;
			if (LeftShift)
			{
				cameraSpeed *= 10.0f;
			}


			glm::vec3 forward = transform.GetRotation() * glm::vec3(0, 0, -1) * cameraSpeed;
			glm::vec3 right = transform.GetRotation() * glm::vec3(1, 0, 0) * cameraSpeed;

			//Using keys
			if (bee::Engine.Input().GetKeyboardKey(bee::Input::KeyboardKey::W))
			{
				transform.SetTranslation(transform.GetTranslation() + forward * m_deltatime);
			}
			if (bee::Engine.Input().GetKeyboardKey(bee::Input::KeyboardKey::A))
			{
				transform.SetTranslation(transform.GetTranslation() - right * m_deltatime);
			}
			if (bee::Engine.Input().GetKeyboardKey(bee::Input::KeyboardKey::S))
			{
				transform.SetTranslation(transform.GetTranslation() - forward * m_deltatime);
			}
			if (bee::Engine.Input().GetKeyboardKey(bee::Input::KeyboardKey::D))
			{
				transform.SetTranslation(transform.GetTranslation() + right * m_deltatime);
			}

			//Using mouse
			if (bee::Engine.Input().GetMouseButtonOnce(bee::Input::MouseButton::Left))
			{
				SaveMousePos = MousePos3D;
			}
			else if (bee::Engine.Input().GetMouseButton(bee::Input::MouseButton::Left))
			{
				glm::vec3 offset = (SaveMousePos - MousePos3D);
				const float limit = 50; // Set limit to not move infinite amount
				if (offset.x + offset.y + offset.z > limit) offset = glm::normalize(offset) * limit;

				transform.SetTranslation(transform.GetTranslation() + offset);
			}

			//------------------------------------------------------------------------------
			//--------------------------Looking around--------------------------------------
			//------------------------------------------------------------------------------
			if (bee::Engine.Input().GetMouseButtonOnce(bee::Input::MouseButton::Right))
			{
				Save2DPos = bee::Engine.Input().GetMousePosition();
				//Save previous roll and pitch outside of loop
				roll = SaveRoll;
				pitch = SavePitch;

			}
			else if (bee::Engine.Input().GetMouseButton(bee::Input::MouseButton::Right))
			{
				//With help from chatGPT

				//---------------------------Set Roll and Pitch for Camera Rotation------------------------
				glm::vec2 mousePos2D = bee::Engine.Input().GetMousePosition();
				SaveRoll = mousePos2D.x - Save2DPos.x;
				SavePitch = mousePos2D.y - Save2DPos.y;

				//Apply sensitivity 
				SaveRoll *= 0.075f;
				SavePitch *= 0.075f;

				//Apply previous roll and pitch
				SaveRoll = SaveRoll + roll;
				SavePitch = SavePitch + pitch;

				if (SavePitch > 89.9f) SavePitch = 89.9f;
				if (SavePitch < -89.9f) SavePitch = -89.9f;

				//Quats for roll and pitch
				glm::quat qRoll = glm::angleAxis(glm::radians(SaveRoll), glm::vec3(0, 1, 0));
				glm::quat qPitch = glm::angleAxis(glm::radians(SavePitch), glm::vec3(1, 0, 0));
				//Combine them
				glm::quat InputRotation = qRoll * qPitch;

				//Set camera rotation
				transform.SetRotation(InputRotation);

			}

			//------------------------------------------------------------------------------
			//--------------------------Zooming in------------------------------------------
			//------------------------------------------------------------------------------

			// if shift is not pressed
			if (!LeftCrtl && MouseWheel != bee::Engine.Input().GetMouseWheel()) //Mouse has been scrolled
			{
				float difference = bee::Engine.Input().GetMouseWheel() - MouseWheel;
				glm::vec3 dir = MousePos3D - transform.GetTranslation();
				dir *= 0.2f; //We do not want to zoom on top of the position, just towards it.
				//Casual P = O + D*T
				transform.SetTranslation(transform.GetTranslation() + dir * difference);
			}
		}
	}

	//------------------------------------------------------------------------------
	//-------------------------Scroll through slices--------------------------------
	//------------------------------------------------------------------------------

	if (m_viewSlice && LeftCrtl && MouseWheel != bee::Engine.Input().GetMouseWheel())
	{
		const int scroll = int(bee::Engine.Input().GetMouseWheel() - MouseWheel);
		int sliceMaxCoord = GRIDSIZESKYX - 1;
		if (m_viewSliceCoord == 0) sliceMaxCoord = GRIDSIZESKYX - 1;
		else if (m_viewSliceCoord == 1) sliceMaxCoord = GRIDSIZESKYY - 1;
		else if (m_viewSliceCoord == 2) sliceMaxCoord = GRIDSIZESKYZ - 1;

		m_atSliceViewSlice = std::clamp(m_atSliceViewSlice + scroll, 0, sliceMaxCoord);
		setSliceMinMax(!m_viewSlice);

	}

}

void editor::setVariables()
{
	//Mouse pos 3D
	MousePos3D = bee::screenToGround(bee::Engine.Input().GetMousePosition());

	//Mouse pos index
	const int pointingAtIdx = tracerObj.getVoxelAtMouse();
	if (pointingAtIdx == -1)
	{
		m_selectionInGrid = false;
	}
	else
	{
		m_selectionInGrid = true;
		m_mousePointingIndex = pointingAtIdx; // Do not change if invalid index
	}
	getCoord(m_mousePointingIndex, m_mousePointingPos.x, m_mousePointingPos.y, m_mousePointingPos.z);

	if (bee::Engine.Input().GetKeyboardKey(bee::Input::KeyboardKey::Space))
	{
		m_skewTidx = m_mousePointingIndex;
		getCoord(m_skewTidx, m_skewTPos.x, m_skewTPos.y, m_skewTPos.z);
	}
}

void editor::mainButtons()
{
	ImGui::BeginGroup();
	if (ImGui::Button(ICON_FA_PLAY))
	{
		m_simulationActive = true;
	}	ImGui::SameLine();
	if (ImGui::Button(ICON_FA_STOP))
	{
		m_simulationActive = false;
	}

	//Step forwards and step backwards (IMPORTANT: step backwards could cause inaccuracies)
	if (ImGui::Button(ICON_FA_ARROW_LEFT) || bee::Engine.Input().GetKeyboardKey(bee::Input::KeyboardKey::ArrowLeft))
	{
		m_simulationStep = -1;
	}	ImGui::SameLine();

	if (ImGui::Button(ICON_FA_ARROW_RIGHT) || bee::Engine.Input().GetKeyboardKey(bee::Input::KeyboardKey::ArrowRight))
	{
		m_simulationStep = 1;
	}
	ImGui::EndGroup();
	ImGui::SameLine();
	ImGui::BeginGroup();
	std::string mode = m_editMode ? "View" : "Edit";
	if (ImGui::Button(mode.c_str()))
	{
		m_editMode = m_editMode ? false : true;
	}
	ImGui::SameLine();
	if (ImGui::Button("SkewTOptions"))
	{
		m_skewTSettings = m_skewTSettings ? false : true;
	}
	if (ImGui::Button("Data"))
	{
		m_dataViewer = m_dataViewer ? false : true;
	}
	ImGui::EndGroup();


}

void editor::viewParamInformation()
{
	int x = m_mousePointingPos.x;
	int y = m_mousePointingPos.y;
	int z = m_mousePointingPos.z;
	const int Gidx = x + z * GRIDSIZESKYX;

	ImGui::Indent();
	ImGui::Text(std::string("X: " + std::to_string(x) + ", Y: " + std::to_string(y) + ", Z: " + std::to_string(z)).c_str());
	ImGui::Text(std::string("Meters Per Voxel: " + std::to_string(VOXELSIZE)).c_str());
	ImGui::Text("--Sky--\nPot Temp: \nQv: \nQw: \nQc: \nQr: \nQs: \nQi: \nWind: \nPressure: \nDebug1: \nDebug2: \nDebug3: \n--Ground--\nTemp: \nWater: \nQr: \nQs: \nQi: "); ImGui::SameLine();
	ImGui::Text(("\n" + //Sky
		std::to_string(m_envData->m_envView.potTemp[m_mousePointingIndex] - 273.15) + "\n" +
		std::to_string(m_envData->m_envView.Qv[m_mousePointingIndex]) + "\n" +
		std::to_string(m_envData->m_envView.Qw[m_mousePointingIndex]) + "\n" +
		std::to_string(m_envData->m_envView.Qc[m_mousePointingIndex]) + "\n" +
		std::to_string(m_envData->m_envView.Qr[m_mousePointingIndex]) + "\n" +
		std::to_string(m_envData->m_envView.Qs[m_mousePointingIndex]) + "\n" +
		std::to_string(m_envData->m_envView.Qi[m_mousePointingIndex]) + "\n" +
		//ImGui::Text((std::string("Wind: ") + std::to_string(getUV(mousePointingIndex).x) + ", " + std::to_string(getUV(mousePointingIndex).y)).c_str());
		std::to_string(m_envData->m_envView.velField[m_mousePointingIndex].x) + ", " + std::to_string(m_envData->m_envView.velField[m_mousePointingIndex].y) + ", " + std::to_string(m_envData->m_envView.velField[m_mousePointingIndex].z) + "\n" +
		std::to_string(m_envData->m_envView.pressure[m_mousePointingIndex]) + "\n" +
		std::to_string(m_envData->m_debugArray0[m_mousePointingIndex]) + "\n" +
		std::to_string(m_envData->m_debugArray1[m_mousePointingIndex]) + "\n" +
		std::to_string(m_envData->m_debugArray2[m_mousePointingIndex]) + "\n" +
		"\n" + //Ground
		std::to_string(m_envData->m_groundView.T[Gidx] - 273.15) + "\n" +
		std::to_string(m_envData->m_groundView.Qrs[Gidx]) + "\n" +
		std::to_string(m_envData->m_groundView.Qgr[Gidx]) + "\n" +
		std::to_string(m_envData->m_groundView.Qgs[Gidx]) + "\n" +
		std::to_string(m_envData->m_groundView.Qgi[Gidx]) + "\n"
		).c_str());

	//Also render a square
	//bee::Engine.DebugRenderer().AddSquare(bee::DebugCategory::All, glm::vec3(float(x) + 0.5f, float(y) + 0.5f, float(z) + 0.5f), 1.0f, glm::vec3(0, 0, 1), bee::Colors::White);
	bee::Engine.DebugRenderer().AddVoxel(bee::DebugCategory::All, glm::vec3(float(x) + 0.5f, float(y) + 0.5f, float(z) + 0.5f), 1.0f, bee::Colors::White);

}

void editor::setView()
{
	if (ImGui::TreeNode("Sky"))
	{
		ImGui::Text("View parameter of:");
		if (ImGui::Button("Temp")) m_viewParamSky = POTTEMP;	ImGui::SameLine();
		if (ImGui::Button("Wind")) m_viewParamSky = WIND;	ImGui::SameLine();
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
		if (ImGui::Button("Temp")) m_viewParamGround = 0;			ImGui::SameLine();
		if (ImGui::Button("WaterContent")) m_viewParamGround = 1;
		if (ImGui::Button("Qr")) m_viewParamGround = 2;				ImGui::SameLine();
		if (ImGui::Button("Qs")) m_viewParamGround = 3;				ImGui::SameLine();
		if (ImGui::Button("Qi")) m_viewParamGround = 4;

		ImGui::TreePop();
	}
	//Resetting
	if (ImGui::TreeNode("Resetting"))
	{
		ImGui::Text("Reset one parameter:");
#if USE_GPU
		if (ImGui::Button("Temp")) Game.EnvGPU().resetParameterGPU(POTTEMP); ImGui::SameLine();
		if (ImGui::Button("Wind")) Game.EnvGPU().resetParameterGPU(WIND); ImGui::SameLine();
		if (ImGui::Button("Ps")) Game.EnvGPU().resetParameterGPU(PRESSURE);
		if (ImGui::Button("Qv")) Game.EnvGPU().resetParameterGPU(QV); ImGui::SameLine();
		if (ImGui::Button("Qw")) Game.EnvGPU().resetParameterGPU(QW); ImGui::SameLine();
		if (ImGui::Button("Qc")) Game.EnvGPU().resetParameterGPU(QC);
		if (ImGui::Button("Qr")) Game.EnvGPU().resetParameterGPU(QR); ImGui::SameLine();
		if (ImGui::Button("Qs")) Game.EnvGPU().resetParameterGPU(QS); ImGui::SameLine();
		if (ImGui::Button("Qi")) Game.EnvGPU().resetParameterGPU(QI);

#else
		if (ImGui::Button("Temp"))
		{
			for (int i = 0; i < GRIDSIZESKY; i++)
			{
				m_envData->m_envView.potTemp[i] = m_envData->m_envTemp[i / GRIDSIZESKYX];  // Could use std::fill
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Wind")) memset(m_envData->m_envView.velField, 0, sizeof(glm::vec2) * GRIDSIZESKY); ImGui::SameLine();
		if (ImGui::Button("Qv"))
		{
			for (int i = 0; i < GRIDSIZESKY; i++)
			{
				m_envData->m_envView.Qv[i] = m_envData->m_envVapor[i / GRIDSIZESKYX]; // Again Could use std::fill
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Qw")) memset(m_envData->m_envView.Qw, 0, sizeof(float) * GRIDSIZESKY); ImGui::SameLine();
		if (ImGui::Button("Qc")) memset(m_envData->m_envView.Qc, 0, sizeof(float) * GRIDSIZESKY);
		if (ImGui::Button("Qr")) memset(m_envData->m_envView.Qr, 0, sizeof(float) * GRIDSIZESKY); ImGui::SameLine();
		if (ImGui::Button("Qs")) memset(m_envData->m_envView.Qs, 0, sizeof(float) * GRIDSIZESKY); ImGui::SameLine();
		if (ImGui::Button("Qi")) memset(m_envData->m_envView.Qi, 0, sizeof(float) * GRIDSIZESKY);
#endif



		ImGui::TreePop();
	}
}

void editor::setSlice()
{
	if (ImGui::TreeNode("View-Slice"))
	{
		if (ImGui::Checkbox("Use Slice View", &m_viewSlice))
		{
			setSliceMinMax(!m_viewSlice);
		}
		
		static int sliceMaxCoord = GRIDSIZESKYX - 1;

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

		ImGui::TreePop();
	}
}

void editor::editModeParams()
{
	if (m_editMode)
	{		
		ImGui::Begin("Edit Variables");

		ImGui::SliderFloat("Simulation Speed", &m_simulationSpeed, 0.1f, 100.0f);
		if (ImGui::Button("Reset Speed")) m_simulationSpeed = 1.0f;

		if (ImGui::TreeNode("Diurnal Cycle"))
		{
			editModeParamsSun();
			ImGui::TreePop();
		}
		if (ImGui::TreeNode("Environment"))
		{

			ImGui::Text(std::string("Currently Editing: " + std::to_string(m_editParamSky)).c_str());
			ImGui::Text("Edit parameter of:");
			if (ImGui::Button("Temp")) m_editParamSky = POTTEMP;		ImGui::SameLine();
			if (ImGui::Button("Wind")) m_editParamSky = WIND;		ImGui::SameLine();
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

			if (e == 0)
			{
				m_brushing = false;
				m_selecting = m_microPhysSelect;
			}
			else if (e == 1)//(ImGui::TreeNode("Brushing"))
			{
				m_brushing = true;
				m_selecting = false;
				m_microPhysSelect = false;

				glm::vec2 minMax = getMinMaxVaueParam(m_editParamSky);
				ImGuiSliderFlags sliderFlag = 0;
				const char* format = getFormatParam(m_editParamSky, sliderFlag);

				ImGui::SliderFloat("Radius", &m_brushSize, 0.5f, 32.0f, "%.3f");
				ImGui::SliderFloat("Smoothness", &m_brushSmoothness, 0.01f, 10.0f, "%.1f");
				ImGui::Text("Value that will be applied every second:");
				ImGui::SliderFloat("AppliedValue", &m_applyValue, minMax.x, minMax.y, format, sliderFlag);
				ImGui::SliderFloat("Intensity", &m_brushIntensity, -1.0f, 1.0f, "%.7f", ImGuiSliderFlags_Logarithmic);

				//Vec3
				if (m_editParamSky == 7)
				{
					vectorArrow();
				}
				else if (m_editParamSky == 8)
				{
					ImGui::Checkbox("erase", &m_groundErase);
				}

				//ImGui::TreePop();
			}
			else if (e == 2)//(ImGui::TreeNode("Selecting"))
			{
				m_brushing = false;
				m_selecting = true;
				m_microPhysSelect = false;

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
		ImGui::End();
	}
}

void editor::editModeParamsSun()
{
	//Time sliderfloat
	float timeHour = m_time / 3600.0f;
	if (ImGui::SliderFloat("Time of day (H)", &timeHour, 0.0f, 24.0f))
	{
		m_timeChanged = true;
	}
	m_time = timeHour * 3600.0f;
	//Pause day cyclus
	ImGui::Checkbox("Pause Diurnal Cycle", &m_pauseDiurnal);
	//Longitude sliderfloat
	ImGui::SliderFloat("Longitude", &m_longitude, 0, 90);
	//Day setting (day/month)
	m_day = chooseDateDay();
	
	//--Extra settings--
	ImGui::SliderFloat("Sun Strength", &m_sunStrength, 0.0f, 10.0f);
}

int editor::chooseDateDay()
{
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

void editor::vectorArrow()
{
	// Using help from Claude AI
	static float hor = 0.0f;   // horizontal angle degrees
	static float ver = 0.0f; // vertical angle degrees

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
	glm::vec3 direction = glm::vec3(
		cosf(v) * sinf(h),
		sinf(v),
		cosf(v) * cosf(h)
	);
	// Make matrix out of it using lookat function
	glm::mat4 viewMat = glm::lookAt(-direction, glm::vec3(0), glm::vec3(0, 1, 0));
	float view[16];
	memcpy(view, glm::value_ptr(viewMat), 16 * sizeof(float));

	ImVec2 gizmoPos = ImGui::GetCursorScreenPos();
	ImVec2 gizmoSize = ImVec2(150, 150);

	// Get right, up and forward from matrix
	glm::mat4 invView = glm::inverse(viewMat);
	glm::vec3 rightDir = glm::vec3(invView[0]);  // X axis — red
	glm::vec3 upDir = glm::vec3(invView[1]);  // Y axis — green
	glm::vec3 forwardDir = glm::vec3(invView[2]);  // Z axis — blue
	ImVec2 center = ImVec2(gizmoPos.x + gizmoSize.x * 0.5f,
		gizmoPos.y + gizmoSize.y * 0.5f);
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
	dl->AddLine(center, tipArrow, IM_COL32(255, 255, 255, 255), 2.0f); // X white
	// Circle on tip
	dl->AddCircleFilled(tipArrow, 4.0f, IM_COL32(255, 255, 255, 255));

	// Helping visualisers
	dl->AddLine(center, tipX, IM_COL32(255, 60, 60, 255), 1.0f); // X red
	dl->AddLine(center, tipY, IM_COL32(60, 255, 60, 255), 1.0f); // Y green
	dl->AddLine(center, tipZ, IM_COL32(60, 60, 255, 255), 1.0f); // Z blue


	ImGui::Text("Direction: %.2f %.2f %.2f", direction.x, direction.y, direction.z);

	//Set data
	m_valueDir = direction;
}

void editor::viewImguiData()
{
	if (bee::Engine.Input().GetKeyboardKey(bee::Input::KeyboardKey::Space))
	{
		viewToolTipData();
	}
	if (bee::Engine.Input().GetKeyboardKey(bee::Input::KeyboardKey::LeftAlt))
	{
		viewPickerData();
	}
}

void editor::setSkewTData()
{
	if (m_skewTSettings)
	{
		ImGui::Begin("SkewT Settings");

		ImGui::SliderFloat2("Pos", &Game.SkewT().skewTPos.x, -360, 360);
		if (ImGui::SliderFloat("Size", &Game.SkewT().skewTSize.x, 1, 1000))
		{
			Game.SkewT().skewTSize.y = Game.SkewT().skewTSize.x;
		}
		if (ImGui::SliderFloat("Skew", &Game.SkewT().tanTheta, 0, 89, "%.0f"))
		{
			Game.SkewT().tanTheta = glm::tan(glm::radians(Game.SkewT().tanTheta));
		}

		ImGui::End();
	}

}

void editor::skewTTexture()
{
	ImGui::Begin("SkewT Draw");

	//Draw onto texture
	glm::vec2 first{ -85, -5 };
	glm::vec2 second{ 5,100 };
	Game.SkewT().getSkewTImage()->getDrawnImage(first, second, texture);

	//Display texture in ImGui
	ImVec2 max = ImGui::GetContentRegionMax();
	max.x = std::min(max.x - 16.0f, max.y - 50.0f);
	max.y = max.x;
	ImTextureID ITID = static_cast<ImTextureID>(texture);
	ImGui::Image(ITID, max, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));

	ImGui::End();
}

void editor::dataClassView()
{
	if (m_dataViewer)
	{
		ImGui::Begin("Data Viewer");

		ImGui::SeparatorText("MicroPhysics Graph");
		if (ImGui::Button("Select Region"))
		{
			m_microPhysSelect = true;
			m_selecting = true;
			m_justViewSelection = false;
		}
		if (ImGui::Button("Cancel Region"))
		{
			m_microPhysSelect = false;
			m_selecting = false;
			m_justViewSelection = false;
		}
		if (ImGui::Button("View Region"))
		{
			m_justViewSelection = true;
			m_selecting = false;
			m_microPhysSelect = false;
		}

		ImGui::End();
	}
}


void editor::viewBackground()
{
	//Set background color based on time;
	const float relativeTime = std::clamp((m_time / 3600 - m_hourOfSunrise) / m_dayLightDuration, 0.0f, 1.0f);
	const float multiplier = std::sin(Constants::PI * relativeTime) + 0.2f;
	bee::Engine.ECS().GetSystem<bee::Renderer>().setBackGroundColor(
		m_backGroundColor[0] * multiplier,
		m_backGroundColor[1] * multiplier,
		m_backGroundColor[2] * multiplier);
}

void editor::viewSky()
{
	auto& colorScheme = bee::Engine.DebugRenderer().GetColorScheme();

	tracerObj.resetGrid(false);

	// Usage of min and max view to possibly use slices
	for (int z = m_minViewZ; z < m_maxViewZ; z++)
	{
		for (int y = m_minViewY; y < m_maxViewY; y++)
		{
			for (int x = m_minViewX; x < m_maxViewX; x++)
			{
				const int idx = getIdx(x, y, z);
				const int idxG = x + z * GRIDSIZESKYX;
				if (y <= m_envData->m_groundHeight[idxG]) 
				{
					tracerObj.setVoxelValue(idx, true); // Add voxel to tracer so we can select it
					if (y == m_envData->m_groundHeight[idxG]) continue; // Leaving 1 voxel space for ground itself
					bee::Engine.DebugRenderer().AddFilledVoxel(bee::DebugCategory::All, glm::vec3(x + 0.5f, y + 0.5f, z + 0.5f), 0.999f, bee::Colors::Brown);
					bee::Engine.DebugRenderer().AddVoxel(bee::DebugCategory::All, glm::vec3(x + 0.5f, y + 0.5f, z + 0.5f), 1.0f, bee::Colors::Black);
					continue;
				}

				glm::vec3 color{};
				switch (m_viewParamSky)
				{
				case POTTEMP:
				{
					//Get temp
					const float Tz = float(m_envData->m_envView.potTemp[idx]) - 273.15f;
					const float T = meteoformulas::potentialTemp(Tz, m_envData->m_groundView.P[idxG], m_envData->m_envView.pressure[idx]) + 273.15f;

					colorScheme.getColor("TemperatureSky", T, color);
					break;
				}
				case QV:
					colorScheme.getColor("mixingRatio", m_envData->m_envView.Qv[idx], color);
					break;
				case QW:
					colorScheme.getColor("mixingRatio", m_envData->m_envView.Qw[idx], color);
					break;
				case QC:
					colorScheme.getColor("mixingRatio", m_envData->m_envView.Qc[idx], color);
					break;
				case QR:
					colorScheme.getColor("mixingRatio", m_envData->m_envView.Qr[idx], color);
					break;
				case QS:
					colorScheme.getColor("mixingRatio", m_envData->m_envView.Qs[idx], color);
					break;
				case QI:
					colorScheme.getColor("mixingRatio", m_envData->m_envView.Qi[idx], color);
					break;
				case WIND:
					const glm::vec3 VelUV = Game.Environment().getUV(m_envData->m_envView.velField, x, y, z);
					// = m_envData->m_envView.velField[x + y * GRIDSIZESKYX];// getUV(idx);
					colorScheme.getColor("velField", glm::length(VelUV), color);
					bee::Engine.DebugRenderer().AddArrow(bee::DebugCategory::All, glm::vec3(x + 0.5f, y + 0.5f, z + 0.5f), glm::vec3(0.0f, 0.0f, 1.0f), VelUV, 0.9f, bee::Colors::Black);
					break;
				case PRESSURE:
					colorScheme.getColor("pressure", m_envData->m_envView.pressure[idx], color);
					break;
				case DEBUG1:
				{
					//Currently for realistic view
					const float allValues = m_envData->m_envView.Qw[idx] + m_envData->m_envView.Qc[idx] +
						m_envData->m_envView.Qr[idx] + m_envData->m_envView.Qs[idx] + m_envData->m_envView.Qi[idx];

					colorScheme.getColor("realistic", allValues, color);
					if (allValues < 0.0001) continue;
					break;
				}
				case DEBUG2:
					colorScheme.getColor("debugColor", m_envData->m_debugArray1[idx], color);
					break;
				case DEBUG3:
					colorScheme.getColor("debugColor", m_envData->m_debugArray2[idx], color);
					break;
				default:
					break;
				}

				//bee::Engine.DebugRenderer().AddSquare(bee::DebugCategory::All, glm::vec3(float(x) + 0.5f, float(y) + 0.5f, 0.0f), 1.0f, glm::vec3(0, 0, 1), bee::Colors::White);
				
				tracerObj.setVoxelValue(idx, true); // Add voxel to tracer so we can select it

				bee::Engine.DebugRenderer().AddFilledVoxel(bee::DebugCategory::All, glm::vec3(x + 0.5f, y + 0.5f, z + 0.5f), 0.999f, { color, 1.0f });
				bee::Engine.DebugRenderer().AddVoxel(bee::DebugCategory::All, glm::vec3(x + 0.5f, y + 0.5f, z + 0.5f), 1.0f, { 0.0f, 0.0f,0.0f, 1.0f });
			}
		}
	}
}

void editor::viewGround()
{
	if (!m_viewGround) return;

	auto& colorScheme = bee::Engine.DebugRenderer().GetColorScheme();

	for (int z = 0; z < GRIDSIZESKYZ; z++)
	{
		for (int x = 0; x < GRIDSIZESKYX; x++)
		{
			const int idx = x + z * GRIDSIZESKYX;
			glm::vec3 color{};
			switch (m_viewParamGround)
			{
			case 0:
				colorScheme.getColor("TemperatureSky", float(m_envData->m_groundView.T[idx]), color);
				break;
			case 1:
				colorScheme.getColor("mixingRatio", m_envData->m_groundView.Qrs[idx], color);
				break;
			case 2:
				colorScheme.getColor("mixingRatio", m_envData->m_groundView.Qgr[idx], color);
				break;
			case 3:
				colorScheme.getColor("mixingRatio", m_envData->m_groundView.Qgs[idx], color);
				break;
			case 4:
				colorScheme.getColor("mixingRatio", m_envData->m_groundView.Qgi[idx], color);
				break;
			}

			bee::Engine.DebugRenderer().AddFilledVoxel(bee::DebugCategory::All, glm::vec3(x + 0.5f, m_envData->m_groundHeight[x + z * GRIDSIZESKYX] + 0.5f, z + 0.5f), 0.999f, { color, 1.0f });
			bee::Engine.DebugRenderer().AddVoxel(bee::DebugCategory::All, glm::vec3(x + 0.5f, m_envData->m_groundHeight[x + z * GRIDSIZESKYX] + 0.5f, z + 0.5f), 0.99f, { 0.0f, 0.0f, 0.0f, 1.0f });

		}
	}
}

void editor::viewBrush()
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

					bee::Engine.DebugRenderer().AddVoxel(bee::DebugCategory::All, glm::vec3(xb, yb, zb) + 0.5f, 1.0f, bee::Colors::White);
				}
			}
		}
	}
}

void editor::viewSelection()
{
	int x = m_mousePointingPos.x;
	int y = m_mousePointingPos.y;
	int z = m_mousePointingPos.z;

	//Set correct selection posses
	if ((m_selecting) && !bee::Engine.Input().GetKeyboardKey(bee::Input::KeyboardKey::LeftShift) && !bee::Engine.Inspector().IsSelected())
	{
		if (bee::Engine.Input().GetMouseButtonOnce(bee::Input::MouseButton::Left))
		{
			m_saveSelectPos = { x, y, z };
			m_selectReset = true;
		}
		else if (bee::Engine.Input().GetMouseButton(bee::Input::MouseButton::Left))
		{
			if (m_saveSelectPos == glm::ivec3(0) && m_selectReset)
			{
				//Impossible (too precise), meaning went from tab to playfield
				m_saveSelectPos = { x, y, z };
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
	else if (bee::Engine.Inspector().IsSelected())
	{
		m_saveSelectPos = glm::ivec3(0);
		m_selectReset = true;
	}
	//Render selection
	if (m_selecting && glm::ivec3(x, y, z) != m_saveSelectPos)
	{
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
					bee::Engine.DebugRenderer().AddVoxel(bee::DebugCategory::All, glm::vec3(sx, sy, sz) + 0.5f, 1.0f, bee::Colors::White);
				}
			}
		}
	}
}

void editor::viewToolTipData()
{
	const int x = m_mousePointingIndex % GRIDSIZESKYX;
	const int y = m_mousePointingIndex / GRIDSIZESKYX;
	const int idx = x + y * GRIDSIZESKYX;

	const float height = y * VOXELSIZE;
	const float Tz = float(m_envData->m_envView.potTemp[idx]) - 273.15f;
	const float T = meteoformulas::potentialTemp(Tz, m_envData->m_groundView.P[int(x)], m_envData->m_envView.pressure[idx]);

	const float rs = meteoformulas::ws(T, m_envData->m_envView.pressure[idx]);
	const float RH = m_envData->m_envView.Qv[m_mousePointingIndex] / rs * 100;
	//Dew point calculation https://www.omnicalculator.com/physics/dew-point
	float dew = 0.0f;
	{
		const float a = 17.625f;
		const float b = 243.04f;
		const float c = log(RH / 100) + a * T / (b + T);
		dew = (b * c) / (a - c);
	}
	ImGui::SetTooltip("T   %.4fC,	 D %.4fC \nRH %.4f%%,	 h %.1fm", T, dew, RH, height);
}

void editor::viewPickerData()
{
	int flag = 0;
	const char* format = getFormatParam(m_viewParamSky, flag);

	std::string pickerString = "Value: " + std::string(format);

	if (m_viewParamSky == WIND)
	{
		pickerString = pickerString + ", " + std::string(format) + ", " + std::string(format);
		ImGui::SetTooltip(pickerString.c_str(), m_applyValue * m_valueDir.x, m_applyValue * m_valueDir.y, m_applyValue * m_valueDir.z);
	}
	else
	{
		ImGui::SetTooltip(pickerString.c_str(), m_applyValue);
	}
}

void editor::viewMicroPhysGraph()
{
	if (m_dataViewer)
	{
		ImGui::Begin("MicroPhysicsGraph");

		Game.DataClass().drawMicroPhysGraph();

		if (ImGui::Button("Reset Graph"))
		{
			Game.DataClass().cancelMicroPhysCheckRegion();
		}
		ImGui::End();
	}
}

void editor::viewSkewT()
{
	float* temps = new float[GRIDSIZESKYY];
	float* dewPoints = new float[GRIDSIZESKYY];
	float* pressures = new float[GRIDSIZESKYY];
	dataToSkewTData(temps, dewPoints, pressures);

	Game.SkewT().setAllArrays(temps, dewPoints, pressures);

	//Small check just in case
	int x = m_skewTPos.x;
	int y = m_skewTPos.y;  
	int z = m_skewTPos.z;
	int idxG = x + z * GRIDSIZESKYX;
	if (y <= m_envData->m_groundHeight[idxG])
	{
		// Set new coords based on valid height
		y = m_envData->m_groundHeight[idxG] + 1;
		m_skewTidx = getIdx(x, y, z);
		m_skewTPos.y = y;
	}
	Game.SkewT().setStartIdx(y);
	Game.SkewT().drawSkewT();


	delete[] temps;
	delete[] dewPoints;
}


void editor::applyBrush()
{

	//printf("brushing: %i, MouseWheel: %f, getMousewheel: %f, !selectedInspector: %i\n", m_brushing, MouseWheel, bee::Engine.Input().GetMouseWheel(), !bee::Engine.Inspector().IsSelected());
	if (m_brushing && MouseWheel != bee::Engine.Input().GetMouseWheel() && !bee::Engine.Inspector().IsSelected())
	{
		const float diff = bee::Engine.Input().GetMouseWheel() - MouseWheel;
		m_brushSize = m_brushSize + diff <= 0.0f ? m_brushSize : m_brushSize + diff;
	}

	if (m_brushing && bee::Engine.Input().GetMouseButton(bee::Input::MouseButton::Left) && !bee::Engine.Inspector().IsSelected())
	{
#if USE_GPU
		Game.EnvGPU().prepareBrushGPU(m_editParamSky, m_brushSize, { m_mousePointingPos.x,m_mousePointingPos.y,m_mousePointingPos.z },
			m_brushSmoothness, m_deltatime, m_brushIntensity, m_applyValue, { m_valueDir.x, m_valueDir.y, m_valueDir.z }, m_groundErase);

#else
		for (int y = int(std::floor(-m_brushSize)); y < int(std::ceil(m_brushSize)); y++)
		{
			for (int x = int(std::floor(-m_brushSize)); x < int(std::ceil(m_brushSize)); x++)
			{
				//Can't brush the ground
				if (m_editParamSky != 8 && y + int(round(MousePos3D.y)) <= m_envData->m_groundHeight[x + int(round(MousePos3D.x))]) continue;

				const float Mx = x + extraX;
				const float My = y + extraY;
				const float distance = Mx * Mx + My * My; //This works since we determine from 0.
				const float radius = m_brushSize * m_brushSize;
				if (distance <= radius) //Within range
				{
					//Based on distance, smoothness and intensity, add value.
					//Distance
					float value1 = -(distance / radius - 1);

					//Smoothness (For now just a simple squared)
					value1 = glm::pow(value1, m_brushSmoothness);

					//Intensity
					value1 *= m_deltatime * m_brushIntensity * m_applyValue; 

					//For directional value
					float value2 = 0.0f;
					if (m_editParamSky == 7)
					{
						value2 = value1 * m_valueDir.y;
						value1 *= m_valueDir.x;
					}
					
					const int Rx = x + int(round(MousePos3D.x));
					const int Ry = y + int(round(MousePos3D.y));


					//Apply
					if (Rx >= 0 && Rx < GRIDSIZESKYX && Ry >= 0 && Ry < GRIDSIZESKYY)
					{
						if (m_editParamSky == 8) //For ground
						{
							setGround(Rx + Ry * GRIDSIZESKYX, m_groundErase);
							continue;
						}

						setValueOfParam(Rx + Ry * GRIDSIZESKYX, m_editParamSky, true, value1, value2);
					}
				}
			}
		}
#endif
	}

}

void editor::applySelect()
{
	if ((m_selecting) && m_saveSelectPos != glm::ivec3(0) && !bee::Engine.Input().GetMouseButton(bee::Input::MouseButton::Left))
	{
		int minX = int(std::min(m_corners[0].x, m_corners[1].x));
		int minY = int(std::min(m_corners[0].y, m_corners[1].y));
		int minZ = int(std::min(m_corners[0].z, m_corners[1].z));
		int maxX = int(std::max(m_corners[0].x, m_corners[1].x));
		int maxY = int(std::max(m_corners[0].y, m_corners[1].y));
		int maxZ = int(std::max(m_corners[0].z, m_corners[1].z));

		if (m_microPhysSelect)
		{
			Game.DataClass().confirmMicroPhysCheckRegion({ minX, minY, minZ }, { maxX, maxY, maxZ });
			m_selecting = false;
			m_microPhysSelect = false;
		}
#if USE_GPU
		Game.EnvGPU().prepareSelectionGPU(m_editParamSky, { minX, minY, minZ }, { maxX, maxY, maxZ }, m_applyValue, { m_valueDir.x, m_valueDir.y, m_valueDir.z }, m_groundErase);

#else
		else
		{
			for (int y = minY; y < maxY; y++)
			{
				for (int x = minX; x < maxX; x++)
				{
					if (x >= 0 && x < GRIDSIZESKYX && y >= 0 && y < GRIDSIZESKYY)
					{
						float value1 = m_applyValue;
						float value2{ 0.0f };

						if (m_editParamSky == 8) //For ground
						{
							setGround(x + y * GRIDSIZESKYX, value1 > 0);
						}
						else if (y > m_envData->m_groundHeight[x])
						{
							if (m_editParamSky == 7)
							{
								value2 = value1 * m_valueDir.y;
								value1 *= m_valueDir.x;
							}
							setValueOfParam(x + y * GRIDSIZESKYX, m_editParamSky, false, value1, value2);
						}
					}
				}
			}
		}
#endif
	}
}

void editor::usePicker()
{
	if (bee::Engine.Input().GetKeyboardKey(bee::Input::KeyboardKey::LeftAlt))
	{
		if (!isOutside(m_mousePointingPos.x, m_mousePointingPos.y, m_mousePointingPos.z))
		{
			glm::vec3 value = getValueParam(m_mousePointingIndex, m_viewParamSky);
			if (m_viewParamSky == WIND)
			{
				//Using normalize and dividing will result in exact value if trying to set value
				m_valueDir = glm::normalize(value);
				m_applyValue = value.x / m_valueDir.x;
			}
			else
			{
				m_applyValue = value.x;
			}
		}
	}

}

void editor::resetValues()
{
	MouseWheel = bee::Engine.Input().GetMouseWheel(); //Update scroll 
}

void editor::setValueOfParam(const int i, const parameter p, const bool add, const float value, const float value2, const float value3)
{
	switch (p)
	{
	case POTTEMP:
		m_envData->m_envView.potTemp[i] = add ? m_envData->m_envView.potTemp[i] + value : value;
		break;
	case QV:
		m_envData->m_envView.Qv[i] = add ? m_envData->m_envView.Qv[i] + value : value;
		break;
	case QW:
		m_envData->m_envView.Qw[i] = add ? m_envData->m_envView.Qw[i] + value : value;
		break;
	case QC:
		m_envData->m_envView.Qc[i] = add ? m_envData->m_envView.Qc[i] + value : value;
		break;
	case QR:
		m_envData->m_envView.Qr[i] = add ? m_envData->m_envView.Qr[i] + value : value;
		break;
	case QS:
		m_envData->m_envView.Qs[i] = add ? m_envData->m_envView.Qs[i] + value : value;
		break;
	case QI:
		m_envData->m_envView.Qi[i] = add ? m_envData->m_envView.Qi[i] + value : value;
		break;
	case WIND:
		m_envData->m_envView.velField[i] = add ? m_envData->m_envView.velField[i] + glm::vec3{ value, value2, value3 } : glm::vec3{ value, value2, value3 };
		break;
	default:
		break;
	}

}

void editor::setGround(const int index, bool ground)
{
	int x = index % GRIDSIZEGROUND;
	int y = index / GRIDSIZEGROUND;

	if (!ground && m_envData->m_groundHeight[x] < y)
	{
		m_changedGround = true;
		m_envData->m_groundHeight[x] = y;
	}
	else if (ground && m_envData->m_groundHeight[x] >= y)
	{
		m_changedGround = true;

		for (int i = m_envData->m_groundHeight[x]; i >= y; i--) //Fill data for current and all above
		{
			addDataErasedGround(x, i);
		}
		m_envData->m_groundHeight[x] = y - 1 < 0 ? 0 : y - 1;
	}
}

void editor::addDataErasedGround(const int x, const int y)
{
	const int idx = x + y * GRIDSIZESKYX;

	//Set data to blank parameters
	m_envData->m_envView.Qv[idx] = m_envData->m_envVapor[y];
	m_envData->m_envView.Qw[idx] = 0.0f;
	m_envData->m_envView.Qc[idx] = 0.0f;
	m_envData->m_envView.Qr[idx] = 0.0f;
	m_envData->m_envView.Qs[idx] = 0.0f;
	m_envData->m_envView.Qi[idx] = 0.0f;
	m_envData->m_envView.potTemp[idx] = m_envData->m_envTemp[y];
	m_envData->m_envView.velField[idx] = { 0, 0, 0 };
	m_envData->m_envView.pressure[idx] = m_envData->m_envPressure[y];
}

void editor::dataToSkewTData(float* temp, float* dew, float* pres)
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
			dew[i]  = 0.0f;
			pres[i] = 0.0f;
			continue;
		}
		const int idx = getIdx(x, i, z);
		const int idxG = x + z * GRIDSIZESKYX;

		const float Tz = float(m_envData->m_envView.potTemp[idx]) - 273.15f;
		const float T = meteoformulas::potentialTemp(Tz, m_envData->m_groundView.P[idxG], m_envData->m_envView.pressure[idx]);

		const float rs = meteoformulas::ws(T, m_envData->m_envView.pressure[idx]);
		const float RH = m_envData->m_envView.Qv[idx] / rs * 100;
		//Dew point calculation https://www.omnicalculator.com/physics/dew-point
		float dewpoint = 0.0f;
		{
			const float a = 17.625f;
			const float b = 243.04f;
			const float c = log(RH / 100) + a * T / (b + T);
			dewpoint = (b * c) / (a - c);
		}

		temp[i] = T;
		dew[i] = RH == 0 ? 0.0f : dewpoint;
		pres[i] = m_envData->m_envView.pressure[idx];
	}
}

glm::vec2 editor::getMinMaxVaueParam(parameter param)
{
	//Return self-set value of parameter
	switch (param)
	{
	case POTTEMP:
		return { 0, 373.15f };
		break;
	case WIND:
		return { -50.0f, 50.0f };
		break;
	case PGROUND:
		break;
	default:
		//All mixing rations
		if (static_cast<int>(param) > 0 && static_cast<int>(param) < 7)
		{
			return { 0.00001, 1.0f };
		}
		break;
	}

	return { 0, 1.0f };
}

const char* editor::getFormatParam(parameter param, int& flagOutput)
{
	flagOutput = 0;

	//Return self-set format of parameter
	switch (param)
	{
	case POTTEMP:
		return "%.2f";
		break;
	case WIND:
		return "%.3f";
		break;
	case PGROUND:
		break;
	default:
		//All mixing rations
		if (static_cast<int>(param) > 0 && static_cast<int>(param) < 7)
		{
			flagOutput = static_cast<int>(ImGuiSliderFlags_Logarithmic);
			return "%.7f";
		}
		//All Debug values
		else if (static_cast<int>(param) > 8 && static_cast<int>(param) < 12)
		{
			return "%.6f";
		}
		break;
	}
	return "%.1f";
}

glm::vec3 editor::getValueParam(const int i, parameter param)
{
	switch (param)
	{
	case POTTEMP:
		return { m_envData->m_envView.potTemp[i], 0.0f, 0.0f };
		break;
	case QV:
		return { m_envData->m_envView.Qv[i], 0.0f, 0.0f };
		break;
	case QW:
		return { m_envData->m_envView.Qw[i], 0.0f, 0.0f };
		break;
	case QC:
		return { m_envData->m_envView.Qc[i], 0.0f, 0.0f };
		break;
	case QR:
		return { m_envData->m_envView.Qr[i], 0.0f, 0.0f };
		break;
	case QS:
		return { m_envData->m_envView.Qs[i], 0.0f, 0.0f };
		break;
	case QI:
		return { m_envData->m_envView.Qi[i], 0.0f, 0.0f };
		break;
	case WIND:
		return { m_envData->m_envView.velField[i].x, m_envData->m_envView.velField[i].y, m_envData->m_envView.velField[i].z };
		break;
	case PGROUND:
		return { 0,0,0 };
		break;
	case DEBUG1:
		return { m_envData->m_debugArray0[i], 0.0f, 0.0f };
		break;
	case DEBUG2:
		return { m_envData->m_debugArray1[i], 0.0f, 0.0f };
		break;
	case DEBUG3:
		return { m_envData->m_debugArray2[i], 0.0f, 0.0f };
		break;
	default:
		break;
	}
	return { 0,0,0 };
}

int editor::getDaysInMonth(int month)
{
	int output = month % 2 ? 30 : 31; //0 = January.
	return month == 1 ? 28 : output;
}

void editor::dayToMonthDay(int dayOfYear, int& month, int& dayOfMonth)
{
	int count = 0;
	for (int i = 0; i < 12; i++)
	{
		dayOfMonth = dayOfYear - count;
		count += getDaysInMonth(i);
		if (count > dayOfYear - 1) {
			month = i;
			return;
		}
	}
}

void editor::setSliceMinMax(bool fullView)
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
		case 0: // Slice X
			m_atSliceViewSlice = std::min(m_atSliceViewSlice, GRIDSIZESKYX - 1);
			m_minViewX = m_atSliceViewSlice;
			m_maxViewX = m_atSliceViewSlice + 1;

			m_minViewY = 0;
			m_minViewZ = 0;
			m_maxViewY = GRIDSIZESKYY;
			m_maxViewZ = GRIDSIZESKYZ;
			break;
		case 1: // Slice Y
			m_atSliceViewSlice = std::min(m_atSliceViewSlice, GRIDSIZESKYY - 1);
			m_minViewY = m_atSliceViewSlice;
			m_maxViewY = m_atSliceViewSlice + 1;

			m_minViewX = 0;
			m_minViewZ = 0;
			m_maxViewX = GRIDSIZESKYX;
			m_maxViewZ = GRIDSIZESKYZ;
			break;
		case 2: // Slice Z
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
