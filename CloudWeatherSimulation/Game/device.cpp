#include "device.h"


#include <glad/include/glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cloud_hub.hpp>
#include <iostream>

// GLFW error callback
void error_callback(int error, const char* description)
{
	fprintf(stderr, "Error: %s\n", description);
}

device::device()
{
	if (!glfwInit())
	{
		printf("Failed to load glfw\n");
		return;
	}

	glfwSetErrorCallback(error_callback);

	// Create window
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#if defined(DEBUG)
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#else
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_FALSE);
#endif
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	m_window = glfwCreateWindow(1960, 1080, "CloudWeatherSimulation Example", NULL, NULL);
	if (!m_window)
	{
		printf("Failed to create a window\n");
		return;
	}
	glfwMakeContextCurrent(m_window);

	// Init glad
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		printf("Failed to load GLAD\n");
		return;
	}
	glfwSwapInterval(1);
}

device::~device()
{
	glfwTerminate();
	glfwDestroyWindow(m_window);
}	

void device::render()
{
	glfwGetFramebufferSize(m_window, &m_width, &m_height);

	glViewport(0, 0, m_width, m_height);

	glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	// GL render loop
	CloudHub.updateMain();
	


	glfwSwapBuffers(m_window);
	glfwPollEvents();
}

bool device::windowShouldClose()
{
	if (m_window)
	{
		return glfwWindowShouldClose(m_window);
	}	
	return true;
}
