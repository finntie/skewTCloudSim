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
#include "environment.h"

#include <sstream>
#include <iostream>
#include <random>


//Game includes

	
using namespace std;
using namespace bee;

gameSystem::gameSystem()
{
	Title = "game";


	//readTableObj.readKNMIFile("assets/input/KNMI/DeBilt_20250301_233612.mwx");
	readTableObj.readDWDFile("assets/input/DWD/sekundenwerte_aero_01303_akt.zip");
	//readTableObj.readDWDFile("assets/input/DWD/sekundenwerte_aero_01303_20240101_20241231_hist.zip");
	readTableObj.initEnvironment();


	//Add light
	Entity lightEntity = Engine.ECS().CreateEntity();
	Transform& transform = Engine.ECS().CreateComponent<Transform>(lightEntity);
	transform.Name = "Light";
	transform.SetTranslation(glm::vec3(0, 0, 10));
	transform.SetRotation(glm::quatLookAt(glm::vec3(0, 0, 0), glm::vec3(0, 0, 1)));
	Engine.ECS().CreateComponent<Light>(lightEntity) = Light(glm::vec3(1.0f), 500.0f, 10000.0f, Light::Type::Directional);
}

gameSystem::~gameSystem()
{
}

//Update function 
void gameSystem::Update(float dt)
{

	//Mouse pos 3D
	MousePos3D = screenToGround(Engine.Input().GetMousePosition());
	Engine.DebugRenderer().AddCircle(DebugCategory::General, MousePos3D, 0.5f, glm::vec4(0, 0, 1, 1), glm::vec4(1.0));
	Engine.DebugRenderer().AddFilledSquare(DebugCategory::General, MousePos3D, 0.5f, glm::vec4(0, 0, 1, 1), glm::vec4(1.0));

	//readTableObj.debugDrawData();

	//printf("3DPos: x: %f, Y: %f\n", MousePos3D.x, MousePos3D.y);

	//if (Engine.Device().ShouldClose()) SLPObj.getMapObj()->exitThread();

	//Camera
	CameraMovement(dt);
}

void gameSystem::Render()
{
}

#ifdef BEE_INSPECTOR

// ImGui integration.
std::string gameSystem::GetName() const { return Title; }
std::string gameSystem::GetIcon() const { return ICON_FA_GAMEPAD; }

void gameSystem::OnPanel()
{
	ImGui::Begin("NewWindow");

	ImGui::SliderFloat("Angle", &readTableObj.angle, 0, 90);
	ImGui::SliderFloat("Liquid", &readTableObj.liquid, 0, 1);
	ImGui::Checkbox("Use i", &readTableObj.useI);

	ImGui::SliderFloat("Width", &readTableObj.sizeSkewT.x, 0.01f, 2.0f);
	ImGui::SliderFloat("Height", &readTableObj.sizeSkewT.y, 0.1f, 100.0f);

	ImGui::SliderFloat("GroundTemp", &readTableObj.skewTData.data.temperature[0], -50.0f, 50.0f);
	ImGui::SliderFloat("GroundDew", &readTableObj.skewTData.data.dewPoint[0], -50.0f, 50.0f);

	ImGui::Text("CAPE: %f", readTableObj.CAPE);

	ImGui::End();
}
#endif

void gameSystem::CameraMovement(float dt)
{

	//Reset state until we de-selected the inspector
	if (Engine.Inspector().IsSelected())
	{
		SaveMousePos = MousePos3D;
		Save2DPos = Engine.Input().GetMousePosition();
		glm::vec2 mousePos2D = Engine.Input().GetMousePosition();
		SaveRoll = mousePos2D.x - Save2DPos.x;
		SavePitch = mousePos2D.y - Save2DPos.y;
		MouseWheel = Engine.Input().GetMouseWheel();
		return;
	}


	//Keybinds = Left-Shift + mouse
	bool LeftShift = Engine.Input().GetKeyboardKey(Input::KeyboardKey::LeftShift);

	//If in view mode, just always be able to move.
	if (!Engine.ECS().GetSystem<environment>().m_editMode)
	{
		LeftShift = true;
	}
	
	//For each camera (we have 1)
	for (const auto& [entity, camera, transform] : Engine.ECS().Registry.view<Camera, Transform>().each())
	{
		//------------------------------------------------------------------------------
		//--------------------------Moving around---------------------------------------
		//------------------------------------------------------------------------------

		float cameraSpeed = 25.0f;
		if (Engine.Input().GetKeyboardKey(Input::KeyboardKey::LeftShift))
		{
			cameraSpeed *= 10.0f;
		}


		//Using keys
		if (Engine.Input().GetKeyboardKey(Input::KeyboardKey::W))
		{
			glm::vec3 offset = { 0,dt * cameraSpeed,0 };
			transform.SetTranslation(transform.GetTranslation() + offset);
		}
		if (Engine.Input().GetKeyboardKey(Input::KeyboardKey::A))
		{
			glm::vec3 offset = { dt * -cameraSpeed,0,0 };
			transform.SetTranslation(transform.GetTranslation() + offset);
		}
		if (Engine.Input().GetKeyboardKey(Input::KeyboardKey::S))
		{
			glm::vec3 offset = { 0,dt * -cameraSpeed,0 };
			transform.SetTranslation(transform.GetTranslation() + offset);
		}
		if (Engine.Input().GetKeyboardKey(Input::KeyboardKey::D))
		{
			glm::vec3 offset = {dt * cameraSpeed,0,0 };
			transform.SetTranslation(transform.GetTranslation() + offset);
		}

		//Using mouse
		if (LeftShift && Engine.Input().GetMouseButtonOnce(Input::MouseButton::Left))
		{
			SaveMousePos = MousePos3D;
		}
		else if (LeftShift && Engine.Input().GetMouseButton(Input::MouseButton::Left))
		{
			glm::vec3 offset = (SaveMousePos - MousePos3D);
			transform.SetTranslation(transform.GetTranslation() + offset);
		}

		//------------------------------------------------------------------------------
		//--------------------------Looking around--------------------------------------
		//------------------------------------------------------------------------------

		if (LeftShift && Engine.Input().GetMouseButtonOnce(Input::MouseButton::Right))
		{
			Save2DPos = Engine.Input().GetMousePosition();
			//Save previous roll and pitch outside of loop
			roll = SaveRoll;
			pitch = SavePitch;

		}
		else if (LeftShift && Engine.Input().GetMouseButton(Input::MouseButton::Right))
		{
			//With help from chatGPT

			//---------------------------Set Roll and Pitch for Camera Rotation------------------------
			glm::vec2 mousePos2D = Engine.Input().GetMousePosition();
			SaveRoll =  mousePos2D.x -  Save2DPos.x;
			SavePitch =  mousePos2D.y - Save2DPos.y ;

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
			glm::quat inputRotation = qRoll * qPitch;

			//Set camera rotation
			transform.SetRotation(inputRotation);	

		}

		//------------------------------------------------------------------------------
		//--------------------------Zooming in------------------------------------------
		//------------------------------------------------------------------------------
		if (!LeftShift)
		{
			//Update scroll if shift is not pressed so it does not register
			MouseWheel = Engine.Input().GetMouseWheel();
		}

		if (MouseWheel != Engine.Input().GetMouseWheel()) //Mouse has been scrolled
		{
			float difference =  Engine.Input().GetMouseWheel() - MouseWheel;
			glm::vec3 dir = MousePos3D - transform.GetTranslation();
			dir *= 0.2f; //We do not want to zoom on top of the position, just towards it.
			//Casual P = O + D*T
			transform.SetTranslation(transform.GetTranslation() + dir * difference);
			MouseWheel = Engine.Input().GetMouseWheel();
		}
	}
}
