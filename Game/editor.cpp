#include "environment.h"
#include "editor.h"

#include "math/meteoformulas.h"
#include "math/constants.hpp"

#include "core/engine.hpp"
#include "imgui/IconsFontAwesome.h"
#include "rendering/colors.hpp"
#include "rendering/debug_render.hpp"
#include "core/input.hpp"
#include "math/geometry.hpp"

#include "imgui/imgui.h"

editor::editor(environment::gridDataSky& skyData, environment::gridDataGround& groundData, int(&height)[GRIDSIZEGROUND], float(&debug1)[GRIDSIZESKY], float (&debug2)[GRIDSIZESKY], float (&debug3)[GRIDSIZESKY])
	: m_envView(skyData), m_groundView(groundData), m_groundHeight(height), m_debugArray0(debug1), m_debugArray1(debug2), m_debugArray2(debug3)
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
	colorScheme.addColor("debugColor", 0.1f, bee::Colors::Pink + glm::vec4(0,0.8f,0,0));
}

void editor::update()
{
	//ImGui
	panel();
	//Edit mode
	editMode();
}

void editor::panel()
{
	mainButtons();
	viewParamInformation();
	setView();

	editModeParams();


}

void editor::viewData()
{
	viewSky();
	viewGround();

	//Mouse pos debugging
	glm::vec3 MousePos3D = bee::screenToGround(bee::Engine.Input().GetMousePosition());
	if (MousePos3D.x < GRIDSIZESKYX && MousePos3D.x >= 0 && MousePos3D.y < GRIDSIZESKYY && MousePos3D.y >= 0)
	{
		m_mousePointingIndex = (int(MousePos3D.x) + int(MousePos3D.y) * GRIDSIZESKYX);
	}
}

void editor::editMode()
{
	m_changedGround = false;

	if (m_editMode)
	{
		viewBrush();
		applyBrush();
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
		std::to_string(m_envView.potTemp[m_mousePointingIndex] - 273.15) + "\n" +
		std::to_string(m_envView.Qv[m_mousePointingIndex]) + "\n" +
		std::to_string(m_envView.Qw[m_mousePointingIndex]) + "\n" +
		std::to_string(m_envView.Qc[m_mousePointingIndex]) + "\n" +
		std::to_string(m_envView.Qr[m_mousePointingIndex]) + "\n" +
		std::to_string(m_envView.Qs[m_mousePointingIndex]) + "\n" +
		std::to_string(m_envView.Qi[m_mousePointingIndex]) + "\n" +
		//ImGui::Text((std::string("Wind: ") + std::to_string(getUV(mousePointingIndex).x) + ", " + std::to_string(getUV(mousePointingIndex).y)).c_str());
		std::to_string(m_envView.velField[m_mousePointingIndex].x) + ", " + std::to_string(m_envView.velField[m_mousePointingIndex].y) + "\n" +
		std::to_string(m_debugArray0[m_mousePointingIndex]) + "\n" +
		std::to_string(m_debugArray1[m_mousePointingIndex]) + "\n" +
		std::to_string(m_debugArray2[m_mousePointingIndex]) + "\n" +
		"\n" + //Ground
		std::to_string(m_groundView.T[m_mousePointingIndex % GRIDSIZESKYX] - 273.15) + "\n" +
		std::to_string(m_groundView.Qrs[m_mousePointingIndex % GRIDSIZESKYX]) + "\n" +
		std::to_string(m_groundView.Qgr[m_mousePointingIndex % GRIDSIZESKYX]) + "\n" +
		std::to_string(m_groundView.Qgs[m_mousePointingIndex % GRIDSIZESKYX]) + "\n" +
		std::to_string(m_groundView.Qgi[m_mousePointingIndex % GRIDSIZESKYX]) + "\n"
		).c_str());
}

void editor::setView()
{
	if (ImGui::TreeNode("Sky"))
	{
		ImGui::Text("View parameter of:");
		if (ImGui::Button("Temp")) m_viewParamSky = 0;	ImGui::SameLine();
		if (ImGui::Button("Wind")) m_viewParamSky = 7;	ImGui::SameLine();
		if (ImGui::Button("Qv"))   m_viewParamSky = 1;		ImGui::SameLine();
		if (ImGui::Button("Qw"))   m_viewParamSky = 2;
		if (ImGui::Button("Qc"))   m_viewParamSky = 3;		ImGui::SameLine();
		if (ImGui::Button("Qr"))   m_viewParamSky = 4;		ImGui::SameLine();
		if (ImGui::Button("Qs"))   m_viewParamSky = 5;		ImGui::SameLine();
		if (ImGui::Button("Qi"))   m_viewParamSky = 6;

		if (ImGui::Button("Debug1")) m_viewParamSky = 8;	  ImGui::SameLine();
		if (ImGui::Button("Debug2")) m_viewParamSky = 9;	  ImGui::SameLine();
		if (ImGui::Button("Debug3")) m_viewParamSky = 10;

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
		if (ImGui::Button("Wind")) memset(m_envView.velField, 0, sizeof(glm::vec2) * GRIDSIZESKY); ImGui::SameLine();
		if (ImGui::Button("Qv")) memset(m_envView.Qv, 0, sizeof(float) * GRIDSIZESKY); ImGui::SameLine();
		if (ImGui::Button("Qw")) memset(m_envView.Qw, 0, sizeof(float) * GRIDSIZESKY); ImGui::SameLine();
		if (ImGui::Button("Qc")) memset(m_envView.Qc, 0, sizeof(float) * GRIDSIZESKY);
		if (ImGui::Button("Qr")) memset(m_envView.Qr, 0, sizeof(float) * GRIDSIZESKY); ImGui::SameLine();
		if (ImGui::Button("Qs")) memset(m_envView.Qs, 0, sizeof(float) * GRIDSIZESKY); ImGui::SameLine();
		if (ImGui::Button("Qi")) memset(m_envView.Qi, 0, sizeof(float) * GRIDSIZESKY);

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

		if (ImGui::TreeNode("Parameters"))
		{
			ImGui::Text(std::string("Currently Editing: " + std::to_string(m_editParamSky)).c_str());
			ImGui::Text("Edit parameter of:");
			if (ImGui::Button("Temp")) m_editParamSky = 0;		ImGui::SameLine();
			if (ImGui::Button("Wind")) m_editParamSky = 7;		ImGui::SameLine();
			if (ImGui::Button("Qv")) m_editParamSky = 1;		ImGui::SameLine();
			if (ImGui::Button("Qw")) m_editParamSky = 2;
			if (ImGui::Button("Qc")) m_editParamSky = 3;		ImGui::SameLine();
			if (ImGui::Button("Qr")) m_editParamSky = 4;		ImGui::SameLine();
			if (ImGui::Button("Qs")) m_editParamSky = 5;		ImGui::SameLine();
			if (ImGui::Button("Qi")) m_editParamSky = 6;
			if (ImGui::Button("Ground")) m_editParamSky = 8;

			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Brushing"))
		{
			ImGui::Checkbox("Brush", &m_brushing);

			ImGui::SliderFloat("Radius", &m_brushSize, 0.5f, 32.0f, "%.3f");
			ImGui::SliderFloat("Smoothness", &m_brushSmoothness, 0.01f, 10.0f, "%.1f");
			ImGui::SliderFloat("Intensity", &m_brushIntensity, 0.000001f, 1.0f, "%.7f", ImGuiSliderFlags_Logarithmic);

			//Vec2
			if (m_editParamSky == 7)
			{
				vectorArrow();
			}
			else if (m_editParamSky == 8)
			{
				ImGui::Checkbox("erase", &m_groundErase);
			}

			ImGui::TreePop();
		}

		ImGui::End();
	}
}

void editor::vectorArrow()
{
	//TODO; for 3D this could be using ImGuizmo.
	static float angle = 0.0f;

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
	m_brushDir = { dirX, -dirY };
}


void editor::viewSky()
{
	auto& colorScheme = bee::Engine.DebugRenderer().GetColorScheme();

	for (int y = 0; y < GRIDSIZESKYY; y++)
	{
		for (int x = 0; x < GRIDSIZESKYX; x++)
		{
			if (y <= m_groundHeight[x])
			{
				bee::Engine.DebugRenderer().AddSquare(bee::DebugCategory::All, glm::vec3(float(x) + 0.5f, float(y) + 0.5f, 0.015f), 1.0f, glm::vec3(0, 0, 1), bee::Colors::Brown);
				continue;
			}

			glm::vec3 color{};
			switch (m_viewParamSky)
			{
			case 0:
			{
				//Get temp
				const float height = y * VOXELSIZE;
				const float pressure = meteoformulas::getStandardPressureAtHeight(float(m_groundView.T[int(x)]), height);
				const float T = float(m_envView.potTemp[int(x) + int(y) * GRIDSIZESKYX]) * glm::pow(pressure / m_groundView.P[int(x)], Constants::Rsd / Constants::Cpd);
				colorScheme.getColor("TemperatureSky", T, color);
				break;
			}
			case 1:
				colorScheme.getColor("mixingRatio", m_envView.Qv[int(x) + int(y) * GRIDSIZESKYX], color);
				break;
			case 2:
				colorScheme.getColor("mixingRatio", m_envView.Qw[int(x) + int(y) * GRIDSIZESKYX], color);
				break;
			case 3:
				colorScheme.getColor("mixingRatio", m_envView.Qc[int(x) + int(y) * GRIDSIZESKYX], color);
				break;
			case 4:
				colorScheme.getColor("mixingRatio", m_envView.Qr[int(x) + int(y) * GRIDSIZESKYX], color);
				break;
			case 5:
				colorScheme.getColor("mixingRatio", m_envView.Qs[int(x) + int(y) * GRIDSIZESKYX], color);
				break;
			case 6:
				colorScheme.getColor("mixingRatio", m_envView.Qi[int(x) + int(y) * GRIDSIZESKYX], color);
				break;
			case 7:
				const glm::vec2 VelUV = m_envView.velField[x + y * GRIDSIZESKYX];// getUV(int(x) + int(y) * GRIDSIZESKYX);
				colorScheme.getColor("velField", glm::length(VelUV), color);
				bee::Engine.DebugRenderer().AddArrow(bee::DebugCategory::All, glm::vec3(x + 0.5f, y + 0.5f, 0.1f), glm::vec3(0.0f, 0.0f, 1.0f), VelUV, 0.9f, bee::Colors::Black);
				break;
			case 8:
				colorScheme.getColor("debugColor", m_debugArray0[int(x) + int(y) * GRIDSIZESKYX], color);
				break;
			case 9:
				colorScheme.getColor("debugColor", m_debugArray1[int(x) + int(y) * GRIDSIZESKYX], color);
				break;
			case 10:
				colorScheme.getColor("debugColor", m_debugArray2[int(x) + int(y) * GRIDSIZESKYX], color);
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
			colorScheme.getColor("TemperatureSky", float(m_groundView.T[x]), color);
			break;
		case 1:
			colorScheme.getColor("mixingRatio", m_groundView.Qrs[int(x)], color);
			break;
		case 2:
			colorScheme.getColor("mixingRatio", m_groundView.Qgr[int(x)], color);
			break;
		case 3:
			colorScheme.getColor("mixingRatio", m_groundView.Qgs[int(x)], color);
			break;
		case 4:
			colorScheme.getColor("mixingRatio", m_groundView.Qgi[int(x)], color);
			break;
		}

		bee::Engine.DebugRenderer().AddSquare(bee::DebugCategory::All, glm::vec3(x + 0.5f, m_groundHeight[x] +  0.5f, 0.0f), 1.0f, glm::vec3(0, 0, 1), bee::Colors::White);
		bee::Engine.DebugRenderer().AddFilledSquare(bee::DebugCategory::All, glm::vec3(x + 0.5f, m_groundHeight[x] + 0.5f, 0.01f), 1.0f, glm::vec3(0, 0, 1), { color, 1.0f });
	}
}

void editor::viewBrush()
{
	if (m_brushing)
	{
		glm::vec3 MousePos3D = bee::screenToGround(bee::Engine.Input().GetMousePosition());
		MousePos3D.z += 0.15f;
		bee::Engine.DebugRenderer().AddCircle(bee::DebugCategory::All, MousePos3D, m_brushSize, glm::vec3(0, 0, 1), bee::Colors::White);
	}
}

void editor::applyBrush()
{
	if (m_brushing && bee::Engine.Input().GetMouseButton(bee::Input::MouseButton::Left))
	{
		glm::vec3 MousePos3D = bee::screenToGround(bee::Engine.Input().GetMousePosition());
		const float extraX = MousePos3D.x - std::floor(MousePos3D.x);
		const float extraY = MousePos3D.y - std::floor(MousePos3D.y);

		for (int y = int(std::floor(-m_brushSize)); y < int(std::ceil(m_brushSize)); y++)
		{
			for (int x = int(std::floor(-m_brushSize)); x < int(std::ceil(m_brushSize)); x++)
			{
				//Can't brush the ground
				if (m_editParamSky != 8 && y + int(round(MousePos3D.y)) <= m_groundHeight[x + int(round(MousePos3D.x))]) continue;

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
					value1 *= 100 * m_deltatime * m_brushIntensity; //TODO: change based on parameter

					//For directional value
					float value2 = 0.0f;
					if (m_editParamSky == 7)
					{
						value2 = value1 * m_brushDir.y;
						value1 *= m_brushDir.x;
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

void editor::setValueOfParam(const int i, const int p, const bool add, const float value, const float value2)
{
	switch (p)
	{
	case 0:
		m_envView.potTemp[i] = add ? m_envView.potTemp[i] + value : value;
		break;
	case 1:
		m_envView.Qv[i] = add ? m_envView.Qv[i] + value : value;
		break;
	case 2:
		m_envView.Qw[i] = add ? m_envView.Qw[i] + value : value;
		break;
	case 3:
		m_envView.Qc[i] = add ? m_envView.Qc[i] + value : value;
		break;
	case 4:
		m_envView.Qr[i] = add ? m_envView.Qr[i] + value : value;
		break;
	case 5:
		m_envView.Qs[i] = add ? m_envView.Qs[i] + value : value;
		break;
	case 6:
		m_envView.Qi[i] = add ? m_envView.Qi[i] + value : value;
		break;
	case 7:
		m_envView.velField[i] = add ? m_envView.velField[i] + glm::vec2{ value, value2 } : glm::vec2{ value, value2 };
		break;
	default:
		break;
	}

}

void editor::setGround(const int index, bool ground)
{
	int x = index % GRIDSIZEGROUND;
	int y = index / GRIDSIZEGROUND;

	if (!ground && m_groundHeight[x] < y)
	{
		m_changedGround = true;
		m_groundHeight[x] = y;
	}
	else if (ground && m_groundHeight[x] >= y)
	{
		m_changedGround = true;

		for (int i = m_groundHeight[x]; i >= y; i--) //Fill data for current and all above
		{
			addDataErasedGround(x, i);
		}
		m_groundHeight[x] = y - 1 < 0 ? 0 : y - 1;
	}
}

void editor::addDataErasedGround(const int x, const int y)
{
	const int idx = x + y * GRIDSIZESKYX;
	const int idxUp = x + (m_groundHeight[x] + 1) * GRIDSIZESKYX;

	//Set data to blank parameters
	m_envView.Qv[idx] = m_envView.Qv[idxUp];
	m_envView.Qw[idx] = m_envView.Qw[idxUp];
	m_envView.Qc[idx] = m_envView.Qc[idxUp];
	m_envView.Qr[idx] = m_envView.Qr[idxUp];
	m_envView.Qs[idx] = m_envView.Qs[idxUp];
	m_envView.Qi[idx] = m_envView.Qi[idxUp];
	m_envView.potTemp[idx] = m_envView.potTemp[idxUp];
	m_envView.velField[idx] = { 0,0 };
}


