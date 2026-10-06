#include "game.h"
#include "device.h"

#include <cloud_hub.hpp>
#include <camera.hpp>

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

game::game()
{
	m_deviceObj = new device();

	CloudHub.initialize();

	// Initialize camera
	glm::vec3 pos = glm::vec3(0, 10, -30);
	glm::vec3 direction = glm::normalize(glm::vec3(0, 0, 0) - pos);
	CloudHub.CameraObj().initialize(m_deviceObj->getWindow(), pos, glm::quatLookAtRH(direction, glm::vec3(0, 1, 0)), glm::perspective(glm::radians(60.0f), 800.0f / 600.0f, 0.1f, 10000.0f));
	CloudHub.setRenderBuffers(0, 0);

}

game::~game()
{
	CloudHub.destruct();
	delete m_deviceObj;
}

void game::core()
{

	while (!m_deviceObj->windowShouldClose())
	{
		// Main game loop

		m_deviceObj->render();
	}

}
