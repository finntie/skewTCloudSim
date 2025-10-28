#include "environment.h"
#include "editor.h"
#include "skewTer.h"
#include "game.h"

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

#include "tools/inspector.hpp"

#include "imgui/IconsFontAwesome.h"
#include "imgui/imgui.h"


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
}

void editor::setIsentropics(float* isentropicTemps, float* isentropicVapor, float* pressures)
{
	//Init skewTer
	{
		float* temps = new float[GRIDSIZESKYY];
		float* dewPoints = new float[GRIDSIZESKYY];

		dataToSkewTData(temps, dewPoints);

		skewTer::skewTInfo skewT;
		skewT.init(GRIDSIZESKYY, temps, dewPoints, pressures);
		m_skewTidx = (m_envData->m_groundHeight[0] + 1) * GRIDSIZESKYX;
		Game.SkewT().setSkewT(skewT);

		delete[] temps;
		delete[] dewPoints;
	}

	memcpy(m_envData->m_envTemp, isentropicTemps, GRIDSIZESKYY * sizeof(float));
	memcpy(m_envData->m_envVapor, isentropicVapor, GRIDSIZESKYY * sizeof(float));
	memcpy(m_envData->m_envPressure, pressures, GRIDSIZESKYY * sizeof(float));

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
	ImGui::End();
	editModeParams();
	setSkewTData();

	viewImguiData();
}

void editor::viewData()
{
	viewBackground();
	viewSky();
	viewGround();
	viewSkewT();
}

void editor::editMode()
{
	m_changedGround = false;

	if (m_editMode)
	{
		viewBrush();
		applyBrush();
		applySelect();
		viewSelection();
		usePicker();
	}

}

void editor::cameraControl()
{
	//Reset state until we de-selected the inspector
#ifdef BEE_INSPECTOR
	if (bee::Engine.Inspector().IsSelected())
	{
		SaveMousePos = MousePos3D;
		Save2DPos = bee::Engine.Input().GetMousePosition();
		glm::vec2 mousePos2D = bee::Engine.Input().GetMousePosition();
		SaveRoll = mousePos2D.x - Save2DPos.x;
		SavePitch = mousePos2D.y - Save2DPos.y;
		MouseWheel = bee::Engine.Input().GetMouseWheel();
		MousePos3D = bee::screenToGround(bee::Engine.Input().GetMousePosition());
		return;
	}
#endif

	//At cursor
	bee::Engine.DebugRenderer().AddCircle(bee::DebugCategory::General, MousePos3D, 0.5f, glm::vec4(0, 0, 1, 1), glm::vec4(1.0));
	bee::Engine.DebugRenderer().AddFilledSquare(bee::DebugCategory::General, MousePos3D, 0.5f, glm::vec4(0, 0, 1, 1), glm::vec4(1.0));

	//Keybinds = Left-Shift + mouse
	bool LeftShift = bee::Engine.Input().GetKeyboardKey(bee::Input::KeyboardKey::LeftShift);

	//If in view mode, just always be able to move.
	if (!m_brushing && !m_selecting)
	{
		LeftShift = true;
	}

	//For each camera (we have 1)
	for (const auto& [entity, camera, transform] : bee::Engine.ECS().Registry.view<bee::Camera, bee::Transform>().each())
	{
		//------------------------------------------------------------------------------
		//--------------------------Moving around---------------------------------------
		//------------------------------------------------------------------------------

		float cameraSpeed = 25.0f;
		if (bee::Engine.Input().GetKeyboardKey(bee::Input::KeyboardKey::LeftShift))
		{
			cameraSpeed *= 10.0f;
		}


		//Using keys
		if (bee::Engine.Input().GetKeyboardKey(bee::Input::KeyboardKey::W))
		{
			glm::vec3 offset = { 0, m_deltatime * cameraSpeed,0 };
			transform.SetTranslation(transform.GetTranslation() + offset);
		}
		if (bee::Engine.Input().GetKeyboardKey(bee::Input::KeyboardKey::A))
		{
			glm::vec3 offset = { m_deltatime * -cameraSpeed,0,0 };
			transform.SetTranslation(transform.GetTranslation() + offset);
		}
		if (bee::Engine.Input().GetKeyboardKey(bee::Input::KeyboardKey::S))
		{
			glm::vec3 offset = { 0,m_deltatime * -cameraSpeed,0 };
			transform.SetTranslation(transform.GetTranslation() + offset);
		}
		if (bee::Engine.Input().GetKeyboardKey(bee::Input::KeyboardKey::D))
		{
			glm::vec3 offset = { m_deltatime * cameraSpeed,0,0 };
			transform.SetTranslation(transform.GetTranslation() + offset);
		}

		//Using mouse
		if (LeftShift && bee::Engine.Input().GetMouseButtonOnce(bee::Input::MouseButton::Left))
		{
			SaveMousePos = MousePos3D;
		}
		else if (LeftShift && bee::Engine.Input().GetMouseButton(bee::Input::MouseButton::Left))
		{
			glm::vec3 offset = (SaveMousePos - MousePos3D);
			transform.SetTranslation(transform.GetTranslation() + offset);
		}

		//------------------------------------------------------------------------------
		//--------------------------Looking around--------------------------------------
		//------------------------------------------------------------------------------

		if (LeftShift && bee::Engine.Input().GetMouseButtonOnce(bee::Input::MouseButton::Right))
		{
			Save2DPos = bee::Engine.Input().GetMousePosition();
			//Save previous roll and pitch outside of loop
			roll = SaveRoll;
			pitch = SavePitch;

		}
		else if (LeftShift && bee::Engine.Input().GetMouseButton(bee::Input::MouseButton::Right))
		{
			//With help from chatGPT

			//---------------------------Set Roll and Pitch for Camera Rotation------------------------
			glm::vec2 mousePos2D = bee::Engine.Input().GetMousePosition();
			SaveRoll = mousePos2D.x - Save2DPos.x;
			SavePitch = mousePos2D.y - Save2DPos.y;

			//Apply sensitivity 
			SaveRoll *= 0.1f;
			SavePitch *= 0.1f;

			//Apply previous roll and pitch
			SaveRoll = SaveRoll + roll;
			SavePitch = SavePitch + pitch;

			if (SavePitch > 179.9f) SavePitch = 179.9f;
			if (SavePitch < 0.5f) SavePitch = 0.5f;

			//Quats for roll and pitch
			glm::quat qRoll = glm::angleAxis(glm::radians(SaveRoll), glm::vec3(0, 0, 1));
			glm::quat qPitch = glm::angleAxis(glm::radians(SavePitch), glm::vec3(1, 0, 0));
			//Combine them
			glm::quat InputRotation = qRoll * qPitch;

			//Set camera rotation
			transform.SetRotation(InputRotation);

		}

		//------------------------------------------------------------------------------
		//--------------------------Zooming in------------------------------------------
		//------------------------------------------------------------------------------
		if (!LeftShift)
		{
			//Update scroll if shift is not pressed so it does not register
			MouseWheel = bee::Engine.Input().GetMouseWheel();
		}

		if (MouseWheel != bee::Engine.Input().GetMouseWheel()) //Mouse has been scrolled
		{
			float difference = bee::Engine.Input().GetMouseWheel() - MouseWheel;
			glm::vec3 dir = MousePos3D - transform.GetTranslation();
			dir *= 0.2f; //We do not want to zoom on top of the position, just towards it.
			//Casual P = O + D*T
			transform.SetTranslation(transform.GetTranslation() + dir * difference);
			MouseWheel = bee::Engine.Input().GetMouseWheel();
		}
	}


}

void editor::setVariables()
{
	//Mouse pos 3D
	MousePos3D = bee::screenToGround(bee::Engine.Input().GetMousePosition());

	//Mouse pos
	if (MousePos3D.x < GRIDSIZESKYX && MousePos3D.x >= 0 && MousePos3D.y < GRIDSIZESKYY && MousePos3D.y >= 0)
	{
		m_mousePointingIndex = (int(MousePos3D.x) + int(MousePos3D.y) * GRIDSIZESKYX);
	}
	if (bee::Engine.Input().GetKeyboardKey(bee::Input::KeyboardKey::Space))
	{
		m_skewTidx = m_mousePointingIndex;
		if (m_skewTidx / GRIDSIZESKYX <= m_envData->m_groundHeight[m_skewTidx % GRIDSIZESKYX])
		{
			m_skewTidx = m_skewTidx % GRIDSIZESKYX + (m_envData->m_groundHeight[m_skewTidx % GRIDSIZESKYX] + 1) * GRIDSIZESKYX;
		}
	}
}

void editor::mainButtons()
{
	if (ImGui::Button(ICON_FA_PLAY))
	{
		m_simulationActive = true;
	}	ImGui::SameLine();

	if (ImGui::Button(ICON_FA_STOP))
	{
		m_simulationActive = false;
	}	ImGui::SameLine();

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

	//Step forwards and step backwards (IMPORTANT: step backwards could cause inaccuracies)
	if (ImGui::Button(ICON_FA_ARROW_LEFT) || bee::Engine.Input().GetKeyboardKey(bee::Input::KeyboardKey::ArrowLeft))
	{
		m_simulationStep = -1;
	}	ImGui::SameLine();

	if (ImGui::Button(ICON_FA_ARROW_RIGHT) || bee::Engine.Input().GetKeyboardKey(bee::Input::KeyboardKey::ArrowRight))
	{
		m_simulationStep = 1;
	}
}

void editor::viewParamInformation()
{
	ImGui::Indent();
	ImGui::Text(std::string("X: " + std::to_string(m_mousePointingIndex % GRIDSIZESKYX) + ", Y: " + std::to_string(int(float(m_mousePointingIndex) / GRIDSIZESKYX))).c_str());
	ImGui::Text(std::string("Meters Per Voxel: " + std::to_string(VOXELSIZE)).c_str());
	ImGui::Text("--Sky--\nPot Temp: \nQv: \nQw: \nQc: \nQr: \nQs: \nQi: \nWind: \nDebug1: \nDebug2: \nDebug3: \n--Ground--\nTemp: \nWater: \nQr: \nQs: \nQi: "); ImGui::SameLine();
	ImGui::Text(("\n" + //Sky
		std::to_string(m_envData->m_envView.potTemp[m_mousePointingIndex] - 273.15) + "\n" +
		std::to_string(m_envData->m_envView.Qv[m_mousePointingIndex]) + "\n" +
		std::to_string(m_envData->m_envView.Qw[m_mousePointingIndex]) + "\n" +
		std::to_string(m_envData->m_envView.Qc[m_mousePointingIndex]) + "\n" +
		std::to_string(m_envData->m_envView.Qr[m_mousePointingIndex]) + "\n" +
		std::to_string(m_envData->m_envView.Qs[m_mousePointingIndex]) + "\n" +
		std::to_string(m_envData->m_envView.Qi[m_mousePointingIndex]) + "\n" +
		//ImGui::Text((std::string("Wind: ") + std::to_string(getUV(mousePointingIndex).x) + ", " + std::to_string(getUV(mousePointingIndex).y)).c_str());
		std::to_string(m_envData->m_envView.velField[m_mousePointingIndex].x) + ", " + std::to_string(m_envData->m_envView.velField[m_mousePointingIndex].y) + "\n" +
		std::to_string(m_envData->m_debugArray0[m_mousePointingIndex]) + "\n" +
		std::to_string(m_envData->m_debugArray1[m_mousePointingIndex]) + "\n" +
		std::to_string(m_envData->m_debugArray2[m_mousePointingIndex]) + "\n" +
		"\n" + //Ground
		std::to_string(m_envData->m_groundView.T[m_mousePointingIndex % GRIDSIZESKYX] - 273.15) + "\n" +
		std::to_string(m_envData->m_groundView.Qrs[m_mousePointingIndex % GRIDSIZESKYX]) + "\n" +
		std::to_string(m_envData->m_groundView.Qgr[m_mousePointingIndex % GRIDSIZESKYX]) + "\n" +
		std::to_string(m_envData->m_groundView.Qgs[m_mousePointingIndex % GRIDSIZESKYX]) + "\n" +
		std::to_string(m_envData->m_groundView.Qgi[m_mousePointingIndex % GRIDSIZESKYX]) + "\n"
		).c_str());

	//Also render a square
	const int x = m_mousePointingIndex % GRIDSIZESKYX;
	const int y = m_mousePointingIndex / GRIDSIZESKYX;

	bee::Engine.DebugRenderer().AddSquare(bee::DebugCategory::All, glm::vec3(float(x) + 0.5f, float(y) + 0.5f, 0.15f), 1.0f, glm::vec3(0, 0, 1), bee::Colors::White);

}

void editor::setView()
{
	if (ImGui::TreeNode("Sky"))
	{
		ImGui::Text("View parameter of:");
		if (ImGui::Button("Temp")) m_viewParamSky = POTTEMP;	ImGui::SameLine();
		if (ImGui::Button("Wind")) m_viewParamSky = WIND;	ImGui::SameLine();
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
			if (ImGui::Button("Ground")) m_editParamSky = GROUND;

			static int e = 0;
			ImGui::RadioButton("Empty", &e, 0); ImGui::SameLine();
			ImGui::RadioButton("Brush", &e, 1); ImGui::SameLine();
			ImGui::RadioButton("Select", &e, 2);

			if (e == 0)
			{
				m_brushing = false;
				m_selecting = false;
			}
			else if (e == 1)//(ImGui::TreeNode("Brushing"))
			{
				m_brushing = true;
				m_selecting = false;

				glm::vec2 minMax = getMinMaxVaueParam(m_editParamSky);
				ImGuiSliderFlags sliderFlag = 0;
				const char* format = getFormatParam(m_editParamSky, sliderFlag);

				ImGui::SliderFloat("Radius", &m_brushSize, 0.5f, 32.0f, "%.3f");
				ImGui::SliderFloat("Smoothness", &m_brushSmoothness, 0.01f, 10.0f, "%.1f");
				ImGui::Text("Value that will be applied every second:");
				ImGui::SliderFloat("AppliedValue", &m_applyValue, minMax.x, minMax.y, format, sliderFlag);
				ImGui::SliderFloat("Intensity", &m_brushIntensity, -1.0f, 1.0f, "%.7f", ImGuiSliderFlags_Logarithmic);

				//Vec2
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
	//TODO; for 3D this could be using ImGuizmo.
	float angle = std::atan2(-m_valueDir.y, m_valueDir.x);;
	ImGui::SliderAngle("Rotation", &angle, 0.0f);

	//Get all angles
	const float dirX = cosf(angle);
	const float dirY = sinf(angle);

	const float pointAngle = 2.6f;
	const float dirXArrow1 = cosf(angle + pointAngle);
	const float dirYArrow1 = sinf(angle + pointAngle);
	const float dirXArrow2 = cosf(angle - pointAngle);
	const float dirYArrow2 = sinf(angle - pointAngle);

	//Set positions
	ImVec2 center = ImGui::GetCursorScreenPos();
	center.x += 30;
	center.y += 30;
	const ImVec2 tip = { center.x + dirX * 25, center.y + dirY * 25 };
	const ImVec2 point1 = { tip.x + dirXArrow1 * 10, tip.y + dirYArrow1 * 10 };
	const ImVec2 point2 = { tip.x + dirXArrow2 * 10, tip.y + dirYArrow2 * 10 };

	//Draw the arrow
	auto* draw = ImGui::GetWindowDrawList();
	draw->AddRectFilled({center.x - 30, center.y - 30}, { center.x + 30, center.y + 30 }, IM_COL32(103, 103, 103, 150), 2.0f);
	draw->AddLine(center, tip, IM_COL32(255, 255, 255, 200), 3.0f);
	draw->AddLine(tip, point1, IM_COL32(255, 255, 255, 200), 3.0f);
	draw->AddLine(tip, point2, IM_COL32(255, 255, 255, 200), 3.0f);

	//Set data
	m_valueDir = { dirX, -dirY };
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

	for (int y = 0; y < GRIDSIZESKYY; y++)
	{
		for (int x = 0; x < GRIDSIZESKYX; x++)
		{
			if (y <= m_envData->m_groundHeight[x])
			{
				bee::Engine.DebugRenderer().AddSquare(bee::DebugCategory::All, glm::vec3(float(x) + 0.5f, float(y) + 0.5f, 0.015f), 1.0f, glm::vec3(0, 0, 1), bee::Colors::Brown);
				continue;
			}

			glm::vec3 color{};
			switch (m_viewParamSky)
			{
			case editor::POTTEMP:
			{
				//Get temp
				const float Tz = float(m_envData->m_envView.potTemp[int(x) + int(y) * GRIDSIZESKYX]) - 273.15f;
				const float T = meteoformulas::potentialTemp(Tz, m_envData->m_groundView.P[int(x)], m_envData->m_envPressure[y]) + 273.15f;

				colorScheme.getColor("TemperatureSky", T, color);
				break;
			}
			case editor::QV:
				colorScheme.getColor("mixingRatio", m_envData->m_envView.Qv[int(x) + int(y) * GRIDSIZESKYX], color);
				break;
			case editor::QW:
				colorScheme.getColor("mixingRatio", m_envData->m_envView.Qw[int(x) + int(y) * GRIDSIZESKYX], color);
				break;
			case editor::QC:
				colorScheme.getColor("mixingRatio", m_envData->m_envView.Qc[int(x) + int(y) * GRIDSIZESKYX], color);
				break;
			case editor::QR:
				colorScheme.getColor("mixingRatio", m_envData->m_envView.Qr[int(x) + int(y) * GRIDSIZESKYX], color);
				break;
			case editor::QS:
				colorScheme.getColor("mixingRatio", m_envData->m_envView.Qs[int(x) + int(y) * GRIDSIZESKYX], color);
				break;
			case editor::QI:
				colorScheme.getColor("mixingRatio", m_envData->m_envView.Qi[int(x) + int(y) * GRIDSIZESKYX], color);
				break;
			case editor::WIND:
				const glm::vec2 VelUV = m_envData->m_envView.velField[x + y * GRIDSIZESKYX];// getUV(int(x) + int(y) * GRIDSIZESKYX);
				colorScheme.getColor("velField", glm::length(VelUV), color);
				bee::Engine.DebugRenderer().AddArrow(bee::DebugCategory::All, glm::vec3(x + 0.5f, y + 0.5f, 0.1f), glm::vec3(0.0f, 0.0f, 1.0f), VelUV, 0.9f, bee::Colors::Black);
				break;
			case editor::DEBUG1:
				colorScheme.getColor("debugColor", m_envData->m_debugArray0[int(x) + int(y) * GRIDSIZESKYX], color);
				break;
			case editor::DEBUG2:
				colorScheme.getColor("debugColor", m_envData->m_debugArray1[int(x) + int(y) * GRIDSIZESKYX], color);
				break;
			case editor::DEBUG3:
				colorScheme.getColor("debugColor", m_envData->m_debugArray2[int(x) + int(y) * GRIDSIZESKYX], color);
				break;
			default:
				break;
			}

			bee::Engine.DebugRenderer().AddSquare(bee::DebugCategory::All, glm::vec3(float(x) + 0.5f, float(y) + 0.5f, 0.0f), 1.0f, glm::vec3(0, 0, 1), bee::Colors::White);
			bee::Engine.DebugRenderer().AddFilledSquare(bee::DebugCategory::All, glm::vec3(float(x) + 0.5f, float(y) + 0.5f, 0.01f), 1.0f, glm::vec3(0, 0, 1), { color, 1.0f });
		}
	}
}

void editor::viewGround()
{
	auto& colorScheme = bee::Engine.DebugRenderer().GetColorScheme();

	for (int x = 0; x < GRIDSIZEGROUND; x++)
	{
		glm::vec3 color{};
		switch (m_viewParamGround)
		{
		case 0:
			colorScheme.getColor("TemperatureSky", float(m_envData->m_groundView.T[x]), color);
			break;
		case 1:
			colorScheme.getColor("mixingRatio", m_envData->m_groundView.Qrs[int(x)], color);
			break;
		case 2:
			colorScheme.getColor("mixingRatio", m_envData->m_groundView.Qgr[int(x)], color);
			break;
		case 3:
			colorScheme.getColor("mixingRatio", m_envData->m_groundView.Qgs[int(x)], color);
			break;
		case 4:
			colorScheme.getColor("mixingRatio", m_envData->m_groundView.Qgi[int(x)], color);
			break;
		}

		bee::Engine.DebugRenderer().AddSquare(bee::DebugCategory::All, glm::vec3(x + 0.5f, m_envData->m_groundHeight[x] +  0.5f, 0.0f), 1.0f, glm::vec3(0, 0, 1), bee::Colors::White);
		bee::Engine.DebugRenderer().AddFilledSquare(bee::DebugCategory::All, glm::vec3(x + 0.5f, m_envData->m_groundHeight[x] + 0.5f, 0.01f), 1.0f, glm::vec3(0, 0, 1), { color, 1.0f });
	}
}

void editor::renderVelSquare(glm::vec2 vel, const int x, const int y)
{
	auto& colorScheme = bee::Engine.DebugRenderer().GetColorScheme();

	glm::vec3 color{};
	colorScheme.getColor("velField", glm::length(vel), color);
	bee::Engine.DebugRenderer().AddArrow(bee::DebugCategory::All, glm::vec3(x + 0.5f, y + 0.5f, 0.1f), glm::vec3(0.0f, 0.0f, 1.0f), vel, 0.9f, bee::Colors::Black);

	bee::Engine.DebugRenderer().AddSquare(bee::DebugCategory::All, glm::vec3(float(x) + 0.5f, float(y) + 0.5f, 0.0f), 1.0f, glm::vec3(0, 0, 1), bee::Colors::White);
	bee::Engine.DebugRenderer().AddFilledSquare(bee::DebugCategory::All, glm::vec3(float(x) + 0.5f, float(y) + 0.5f, 0.01f), 1.0f, glm::vec3(0, 0, 1), { color, 1.0f });
}

void editor::viewBrush()
{
	if (m_brushing)
	{
		MousePos3D.z += 0.15f;
		bee::Engine.DebugRenderer().AddCircle(bee::DebugCategory::All, MousePos3D, m_brushSize, glm::vec3(0, 0, 1), bee::Colors::White);
	}
}

void editor::viewSelection()
{
	//Set correct selection posses
	if (m_selecting && !bee::Engine.Input().GetKeyboardKey(bee::Input::KeyboardKey::LeftShift) && !bee::Engine.Inspector().IsSelected())
	{
		if (bee::Engine.Input().GetMouseButtonOnce(bee::Input::MouseButton::Left))
		{
			m_saveSelectPos = { (MousePos3D.x), (MousePos3D.y) };
			m_selectReset = true;
		}
		else if (bee::Engine.Input().GetMouseButton(bee::Input::MouseButton::Left))
		{
			if (m_saveSelectPos == glm::vec2(0) && m_selectReset)
			{
				//Impossible (too precise), meaning went from tab to playfield
				m_saveSelectPos = { (MousePos3D.x), (MousePos3D.y) };
			}

			m_corners[0].x = MousePos3D.x < m_saveSelectPos.x ? ceil(m_saveSelectPos.x) : floor(m_saveSelectPos.x);
			m_corners[0].y = MousePos3D.y < m_saveSelectPos.y ? ceil(m_saveSelectPos.y) : floor(m_saveSelectPos.y);

			m_corners[1].x = MousePos3D.x < m_saveSelectPos.x ? floor(MousePos3D.x) : ceil(MousePos3D.x);
			m_corners[1].y = MousePos3D.y < m_saveSelectPos.y ? floor(MousePos3D.y) : ceil(MousePos3D.y);

			m_selectReset = false;
		}
		else if (m_selectReset)
		{
			m_saveSelectPos = { 0,0 };
		}
	}
	else if (bee::Engine.Inspector().IsSelected())
	{
		m_saveSelectPos = { 0,0 };
		m_selectReset = true;
	}
	//Render selection
	if (m_selecting && glm::vec2(MousePos3D.x, MousePos3D.y) != m_saveSelectPos)
	{
		bee::Engine.DebugRenderer().AddRectangle(bee::DebugCategory::All, glm::vec3(m_corners[0], 0.015f), glm::vec3(m_corners[1], 0.015f), glm::vec3(0, 0, 1), bee::Colors::White);
	}

}

void editor::viewToolTipData()
{
	const int x = m_mousePointingIndex % GRIDSIZESKYX;
	const int y = m_mousePointingIndex / GRIDSIZESKYX;

	const float height = y * VOXELSIZE;
	const float Tz = float(m_envData->m_envView.potTemp[int(x) + int(y) * GRIDSIZESKYX]) - 273.15f;
	const float T = meteoformulas::potentialTemp(Tz, m_envData->m_groundView.P[int(x)], m_envData->m_envPressure[y]);

	const float rs = meteoformulas::ws(T, m_envData->m_envPressure[y]);
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
		pickerString = pickerString + ", " + std::string(format);
		ImGui::SetTooltip(pickerString.c_str(), m_applyValue * m_valueDir.x, m_applyValue * m_valueDir.y);
	}
	else
	{
		ImGui::SetTooltip(pickerString.c_str(), m_applyValue);
	}
}

void editor::viewSkewT()
{
	float* temps = new float[GRIDSIZESKYY];
	float* dewPoints = new float[GRIDSIZESKYY];
	dataToSkewTData(temps, dewPoints);

	Game.SkewT().setAllArrays(temps, dewPoints, m_envData->m_envPressure);
	Game.SkewT().setStartIdx(m_skewTidx / GRIDSIZESKYX);
	Game.SkewT().drawSkewT();


	delete[] temps;
	delete[] dewPoints;
}


void editor::applyBrush()
{
	if (m_brushing && bee::Engine.Input().GetMouseButton(bee::Input::MouseButton::Left) && !bee::Engine.Inspector().IsSelected())
	{
		const float extraX = MousePos3D.x - std::floor(MousePos3D.x);
		const float extraY = MousePos3D.y - std::floor(MousePos3D.y);

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
	}

}

void editor::applySelect()
{
	if (m_selecting && m_saveSelectPos != glm::vec2(0) && !bee::Engine.Input().GetMouseButton(bee::Input::MouseButton::Left))
	{
		int minX = int(std::min(m_corners[0].x, m_corners[1].x));
		int minY = int(std::min(m_corners[0].y, m_corners[1].y));
		int maxX = int(std::max(m_corners[0].x, m_corners[1].x));
		int maxY = int(std::max(m_corners[0].y, m_corners[1].y));

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
}

void editor::usePicker()
{
	if (bee::Engine.Input().GetKeyboardKey(bee::Input::KeyboardKey::LeftAlt))
	{
		if (MousePos3D.x < GRIDSIZESKYX && MousePos3D.x >= 0 && MousePos3D.y < GRIDSIZESKYY && MousePos3D.y >= 0)
		{
			glm::vec2 value = getValueParam(m_mousePointingIndex, m_viewParamSky);
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

void editor::setValueOfParam(const int i, const parameter p, const bool add, const float value, const float value2)
{
	switch (p)
	{
	case editor::POTTEMP:
		m_envData->m_envView.potTemp[i] = add ? m_envData->m_envView.potTemp[i] + value : value;
		break;
	case editor::QV:
		m_envData->m_envView.Qv[i] = add ? m_envData->m_envView.Qv[i] + value : value;
		break;
	case editor::QW:
		m_envData->m_envView.Qw[i] = add ? m_envData->m_envView.Qw[i] + value : value;
		break;
	case editor::QC:
		m_envData->m_envView.Qc[i] = add ? m_envData->m_envView.Qc[i] + value : value;
		break;
	case editor::QR:
		m_envData->m_envView.Qr[i] = add ? m_envData->m_envView.Qr[i] + value : value;
		break;
	case editor::QS:
		m_envData->m_envView.Qs[i] = add ? m_envData->m_envView.Qs[i] + value : value;
		break;
	case editor::QI:
		m_envData->m_envView.Qi[i] = add ? m_envData->m_envView.Qi[i] + value : value;
		break;
	case editor::WIND:
		m_envData->m_envView.velField[i] = add ? m_envData->m_envView.velField[i] + glm::vec2{ value, value2 } : glm::vec2{ value, value2 };
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
	m_envData->m_envView.velField[idx] = { 0,0 };
}


void editor::dataToSkewTData(float* temp, float* dew)
{
	const int y = m_skewTidx / GRIDSIZESKYX;
	const int x = m_skewTidx % GRIDSIZESKYX;
	for (int i = 0; i < GRIDSIZESKYY; i++)
	{
		if (i < y)
		{
			temp[i] = 0;
			dew[i] = 0;
			continue;
		}

		const float Tz = float(m_envData->m_envView.potTemp[x + i * GRIDSIZESKYX]) - 273.15f;
		const float T = meteoformulas::potentialTemp(Tz, m_envData->m_groundView.P[int(x)], m_envData->m_envPressure[i]);

		const float rs = meteoformulas::ws(T, m_envData->m_envPressure[i]);
		const float RH = m_envData->m_envView.Qv[x + i * GRIDSIZESKYX] / rs * 100;
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
	}
}

glm::vec2 editor::getMinMaxVaueParam(parameter param)
{
	//Return self-set value of parameter
	switch (param)
	{
	case editor::POTTEMP:
		return { 0, 373.15f };
		break;
	case editor::WIND:
		return { -50.0f, 50.0f };
		break;
	case editor::GROUND:
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
	case editor::POTTEMP:
		return "%.2f";
		break;
	case editor::WIND:
		return "%.3f";
		break;
	case editor::GROUND:
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

glm::vec2 editor::getValueParam(const int i, parameter param)
{
	switch (param)
	{
	case editor::POTTEMP:
		return { m_envData->m_envView.potTemp[i], 0.0f };
		break;
	case editor::QV:
		return { m_envData->m_envView.Qv[i], 0.0f };
		break;
	case editor::QW:
		return { m_envData->m_envView.Qw[i], 0.0f };
		break;
	case editor::QC:
		return { m_envData->m_envView.Qc[i], 0.0f };
		break;
	case editor::QR:
		return { m_envData->m_envView.Qr[i], 0.0f };
		break;
	case editor::QS:
		return { m_envData->m_envView.Qs[i], 0.0f };
		break;
	case editor::QI:
		return { m_envData->m_envView.Qi[i], 0.0f };
		break;
	case editor::WIND:
		return { m_envData->m_envView.velField[i].x, m_envData->m_envView.velField[i].y };
		break;
	case editor::GROUND:
		return { 0,0 };
		break;
	case editor::DEBUG1:
		return { m_envData->m_debugArray0[i], 0.0f };
		break;
	case editor::DEBUG2:
		return { m_envData->m_debugArray1[i], 0.0f };
		break;
	case editor::DEBUG3:
		return { m_envData->m_debugArray2[i], 0.0f };
		break;
	default:
		break;
	}
	return { 0,0 };
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
