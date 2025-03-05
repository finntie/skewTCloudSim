#pragma once
#include <glm/glm.hpp>
#include "core/ecs.hpp"
#include "tools/inspectable.hpp"
#include "readTable.h"

using namespace bee; 

class gameSystem : public bee::System, public bee::IPanel
{
public:
	gameSystem();
	~gameSystem();
	void Update(float dt) override;
	void Render() override;
	void CameraMovement(float dt);
#ifdef BEE_INSPECTOR

	void OnPanel() override;
	std::string GetName() const override;
	std::string GetIcon() const override;
#endif

private:
	
	readTable readTableObj;

	//Camera variables
	glm::vec3 MousePos3D{};
	glm::vec3 SaveMousePos = glm::vec3(0);
	glm::vec2 Save2DPos = glm::vec2(0);
	float roll = 0.0f, pitch = 0.0f;
	float SaveRoll = 0.0f, SavePitch = 0.0f;
	float MouseWheel = 0;


};

