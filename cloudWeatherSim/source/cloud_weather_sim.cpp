#include <cloudWeatherSim/cloud_weather_sim.hpp>

#include <cloud_hub.hpp>
#include <camera.hpp>
#include <cloud_menu_hub.hpp>
#include <rendering/render_hub.hpp>
#include <rendering/render_hub.hpp>
#include "rendering/cuda/cuda_render_gl.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>

bool mainUpdateOnlyView = false;

void cloudWeatherSim::init(GLFWwindow* window, cloudWeatherSimInitFlags flags)
{
	CloudHub.CameraObj().initDevice(window);

	CloudHub.initialize(flags & initFlag_LibImGuiStyle);

	CloudHub.CameraObj().setCameraLib(flags & initFlag_UseLibInput);

	// Dont show ui
	if (flags & initFlag_NoUIOnlyCloud) mainUpdateOnlyView = true;

	// Dont draw the final result
	if (flags & initFlag_DontDrawFinalResult) CloudHub.CloudRender().cudaRendererObj().setDrawFinalResult(false);
}

void cloudWeatherSim::destruct()
{
	CloudHub.destruct();
}

void cloudWeatherSim::update()
{
	CloudHub.updateMain(mainUpdateOnlyView);
}

void cloudWeatherSim::setRenderBuffers(unsigned int framebuffer, unsigned int colorbuffer)
{
	CloudHub.CloudRender().init(framebuffer, colorbuffer);
}

void cloudWeatherSim::initCamera(float* projection)
{
	CloudHub.CameraObj().initUserCamera(glm::make_mat4(projection));
}

void cloudWeatherSim::updateCamera(float* pos, float* rotation)
{
	CloudHub.CameraObj().updateCamera(glm::make_vec3(pos), glm::make_quat(rotation));
}

void cloudWeatherSim::updateUIImGui()
{
	if (!mainUpdateOnlyView) CloudHub.cloudMenuObj().update();
}

void cloudWeatherSim::loadCloudFile(const char* file)
{
	mainUpdateOnlyView = true;
	CloudHub.CloudRender().loadFile(file);
}

void cloudWeatherSim::loadRenderSettingsFile(const char* file)
{
	CloudHub.CloudRender().loadRenderSettingsFile(file);
}

unsigned int cloudWeatherSim::getRenderFinalTexture()
{
	return CloudHub.CloudRender().cudaRendererObj().getFinalTexture();
}