#include "pch.h"
#include "gameSystem.h"

#include "skewTMaker.h"
#include "readTable.h"
#include "environment.h"
#include "editor.h"
#include "skewTer.h"
#include "game.h"

//Game includes

	
using namespace std;
using namespace bee;


gameSystem::gameSystem()
{
	Title = "game";
	
	Game.Initialize();

	Game.Editor().setColors();
	//Game.ReadTable().readKNMIFile("assets/input/KNMI/DeBilt_20250807_113122.mwx");
	Game.ReadTable().readDWDFile("assets/input/DWD/sekundenwerte_aero_01303_akt.zip");
	//Game.ReadTable().readDWDFile("assets/input/DWD/sekundenwerte_aero_01303_20240101_20241231_hist.zip");

	Game.ReadTable().initEnvironment();
}


gameSystem::~gameSystem() {}

//Update function 
void gameSystem::Update(float dt)
{
	if (makingSkewT)
	{
		Game.SkewTMaker().update(dt);
		if (Game.SkewTMaker().doneMakingSkewT)
		{
			makingSkewT = false;
			loaded = true;
		}
	}
	if (loaded)
	{
		Game.Update(dt);
	}
	//Game.ReadTable().debugDrawData();
}

void gameSystem::Render() {}

// ImGui integration.
std::string gameSystem::GetName() const { return Title; }
std::string gameSystem::GetIcon() const { return ICON_FA_GAMEPAD; }


void gameSystem::OnPanel()
{
	if (makingSkewT)
	{
		Game.SkewTMaker().panel();
	}
	else if (loaded)
	{
		Game.Editor().panel();
	}
	else
	{
		startMenu();
	}

	//ImGui::Begin("NewWindow");
	//
	//ImGui::SliderFloat("Angle", &Game.ReadTable().angle, 0, 90);
	//ImGui::SliderFloat("Liquid", &Game.ReadTable().liquid, 0, 1);
	//ImGui::Checkbox("Use i", &Game.ReadTable().useI);
	//
	//ImGui::SliderFloat("Width", &Game.ReadTable().sizeSkewT.x, 0.01f, 2.0f);
	//ImGui::SliderFloat("Height", &Game.ReadTable().sizeSkewT.y, 0.1f, 100.0f);
	//
	//ImGui::SliderFloat("GroundTemp", &Game.ReadTable().skewTData.data.temperature[0], -50.0f, 50.0f);
	//ImGui::SliderFloat("GroundDew", &Game.ReadTable().skewTData.data.dewPoint[0], -50.0f, 50.0f);
	//
	//ImGui::Text("CAPE: %f", Game.ReadTable().CAPE);
	//
	//ImGui::End();
}

void gameSystem::startMenu()
{
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.3f, 0.3f, 0.7f, 1.0f));

	ImGui::Begin("StartMenu", 0, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar);

	// Set window settings
	ImGui::SetWindowPos(ImVec2(0,0));
	auto io = ImGui::GetIO();
	ImGui::SetWindowSize(io.DisplaySize);
	ImGui::SetWindowFocus();
	ImVec2 centerOfScreen = ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);

	// Button customization
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1, 1, 1, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.45f, 0.3f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.55f, 0.4f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.3f, 0.15f, 1.0f));

	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);


	// Actual buttons
	ImGui::SetCursorPos(ImVec2(centerOfScreen.x - 175, centerOfScreen.y - 200));
	if (ImGui::Button("Create Skew-T", ImVec2(350, 100)))
	{
		Game.SkewTMaker().init();
		makingSkewT = true;
	}
	ImGui::SetCursorPos(ImVec2(centerOfScreen.x - 175, centerOfScreen.y));
	if (ImGui::Button("Load Observed Skew-T", ImVec2(350, 100)))
	{
		loaded = true;
	}


	ImGui::PopStyleColor(4);
	ImGui::PopStyleVar(2);

	ImGui::End();

	ImGui::PopStyleColor();
}