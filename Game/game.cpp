#include "game.h"
#include "device.h"

#include <cloudWeatherSim/cloud_weather_sim.hpp>

#include <imgui/imgui.h>
#include <imgui/implot.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>
#include <imgui/IconsFontAwesome.h>

#include <string>
game::game()
{

	m_deviceObj = new device();

	imguiInitTest();
	cloudWeatherSim::init(m_deviceObj->getWindow(), cloudWeatherSim::initFlag_UseLibInput);
}

game::~game()
{
	cloudWeatherSim::destruct();
	delete m_deviceObj;
}

void game::core()
{

	//cloudWeatherSim::loadCloudFile("singleCloudFile2.bin");
	//cloudWeatherSim::loadRenderSettingsFile("32Settings.bin");

	while (!m_deviceObj->windowShouldClose())
	{


		m_deviceObj->render();

	}

}

void game::imguiInitTest()
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