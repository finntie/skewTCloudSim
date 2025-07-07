#include "editor.h"

#include "environment.h"

#include "core/engine.hpp"
#include "imgui/IconsFontAwesome.h"
#include "rendering/colors.hpp"
#include "rendering/debug_render.hpp"
#include "core/input.hpp"

#include "imgui/imgui.h"

editor::editor()
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

	colorScheme.createColorScheme("debugColor", -1, bee::Colors::Blue, 1, bee::Colors::Red);
	colorScheme.addColor("debugColor", -0.001f, bee::Colors::DodgerBlue);
	colorScheme.addColor("debugColor", -0.0001f, bee::Colors::Cyan);
	colorScheme.addColor("debugColor", 0.0f, bee::Colors::Green);
	colorScheme.addColor("debugColor", 0.0001f, bee::Colors::Yellow);
	colorScheme.addColor("debugColor", 0.001f, bee::Colors::Orange);
}

void editor::panel()
{
	mainButtons();





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


