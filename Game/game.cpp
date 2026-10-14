#include "game.h"
#include "device.h"

#include <cloudWeatherSim/cloud_weather_sim.hpp>

#include <imgui/imgui.h>
#include <imgui/implot.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>
#include <imgui/IconsFontAwesome.h>

#include <string>

#include <filesystem>
#include <iostream>
game::game()
{

	m_deviceObj = new device();

	imguiInit();
	cloudWeatherSim::init(m_deviceObj->getWindow(), cloudWeatherSim::initFlag_UseLibInput | cloudWeatherSim::initFlag_LibImGuiStyle);
}

game::~game()
{
	cloudWeatherSim::destruct();
	delete m_deviceObj;
}

void game::core()
{
	cloudWeatherSim::loadCloudFile("singleCloudFile2.bin");
	//cloudWeatherSim::loadRenderSettingsFile("32Settings.bin");

	while (!m_deviceObj->windowShouldClose())
	{
		m_deviceObj->renderInit();

		cloudWeatherSim::update();
		imguiUpdate();

		m_deviceObj->renderFinish();
	}

}

void game::imguiInit()
{
    ImGuiContext* cont =  ImGui::CreateContext();
	ImGui::SetCurrentContext(cont);
    ImPlot::CreateContext();

    const auto window = m_deviceObj->getWindow();
    const bool opengl = ImGui_ImplOpenGL3_Init();
    const bool glfw = ImGui_ImplGlfw_InitForOpenGL(window, true);

	ImGuiIO& io = ImGui::GetIO();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
}

void game::imguiUpdate()
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	ImGui::Begin("Game");
	cloudWeatherSim::updateUIImGui();
	ImGui::End();

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
