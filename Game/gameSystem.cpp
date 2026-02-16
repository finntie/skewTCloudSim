#include "gameSystem.h"

//Engine
#include "core/engine.hpp"
#include "core/input.hpp"
#include "core/transform.hpp"
#include "rendering/debug_render.hpp"
#include "rendering/render.hpp"
#include "core/resources.hpp"
#include "rendering/model.hpp"
#include "rendering/mesh.hpp"
#include "imgui/imgui.h"
#include "tools/profiler.hpp"
#include "core/device.hpp"
#include "tools/inspector.hpp"
#include "math/geometry.hpp"

#include "readTable.h"
#include "environment.h"
#include "editor.h"
#include "skewTer.h"
#include "game.h"

#include <sstream>
#include <iostream>
#include <random>


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
	Game.Update(dt);
	//Game.ReadTable().debugDrawData();
}

void gameSystem::Render() {}

// ImGui integration.
std::string gameSystem::GetName() const { return Title; }
std::string gameSystem::GetIcon() const { return ICON_FA_GAMEPAD; }

void gameSystem::OnPanel()
{
	Game.Editor().panel();
	
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
