#pragma once

struct GLFWwindow;

namespace cloudWeatherSim
{

	int typedef cloudWeatherSimInitFlags; // From Enum:  cloudWeatherSimInitFlags_


	enum cloudWeatherSimInitFlags_
	{
		initFlag_None = 0 << 0,
		initFlag_UseLibInput = 1 << 0, // Use the library's input instead of own when moving around the clouds. If using own input, you will need to call initCamera() and afterwards updateCamera() each frame.
		initFlag_NoUIOnlyCloud = 1 << 1, // Only view the clouds after loading it yourself using loadCloudFile()
		initFlag_LibImGuiStyle = 1 << 2, // Use the ImGui style which the library uses, else it will not be set/used
		initFlag_DontDrawFinalResult = 1 << 3 // Hold off on drawing the final result inside the library and possibly use the final texture; 'getRenderFinalTexture()' to draw yourself
	};


	/// <summary>
	/// Initializes certain objects of the library which depend on OpenGL/GLFW already being set up. 
	/// Make sure to call this After your OpenGL/GLFW is being set up, same goes for ImGui (if you have one). 
	/// Don't forget to call destruct after finishing with the library
	/// </summary>
	/// <param name="window">GLFW window, this will be used to set own callbacks for window resizing and input</param>
	/// <param name="flags">Initialize settings, includes flags like usage of own input, or hide UI.</param>
	void init(GLFWwindow* window, cloudWeatherSimInitFlags flags = initFlag_None);

	/// <summary>
	/// Correctly destructs the library safely.
	/// </summary>
	void destruct();

	/// <summary>
	/// Updates the library, handles its own input.
	/// If using the buffers to draw geometry, make sure your scene is drawn onto the buffers before calling this update()
	/// </summary>
	void update();

	/// <summary>
	/// To combine the cloud rendering together with the user's rendering, we need the frame buffer and color buffer
	/// </summary>
	/// <param name="framebuffer"> OpenGL framebuffer, should contain at least a depth buffer. </param>
	/// <param name="colorbuffer"> OpenGL color buffer, (not yet) support for RGB and RGBA</param>
	void setRenderBuffers(unsigned int framebuffer, unsigned int colorbuffer);

	/// <summary>
	/// Initializes the projection matrix to set correct camera settings.
	/// <para>IMPORTANT: it is recommended to use a pretty big far camera plane, with the big clouds visuals together with possibly unrenderer geometry. </para>
	/// <para>IMPORTANT: The library expects log scaled depth buffer. If unfimiliar take a note to https://stackoverflow.com/questions/18182139/logarithmic-depth-buffer-linearization/18187212#18187212 </para>
	/// </summary>
	/// <param name="projection">glm::mat4 Camera Projection Matrix</param>
	void initCamera(float* projection);

	/// <summary>
	/// Update camera position, expects #useLibInput being set to false from the cloudWeatherSim::init() function.
	/// </summary>
	/// <param name="pos">glm::vec3 Camera position</param>
	/// <param name="rotation">glm::quat Camera rotation as quaternion</param>
	void updateCamera(float* pos, float* rotation);

	/// <summary>
	/// Renders the UI from this library using ImGui. 
	/// ImGui has to be set up correctly and this function needs to be called in between ImGui::NewFrame() and ImGui::Render(). 
	/// To get the correct ImGui style, use the 'initFlag_LibImGuiStyle' flag in the init
	/// </summary>
	void updateUIImGui();

	/// <summary>
	/// Loads a cloud file, also makes sure that when updating, it will only update to view and not any UI.
	/// </summary>
	/// <param name="file">File name only to saved cloud file, assumes location assets/output/</param>
	void loadCloudFile(const char* file);

	/// <summary>
	/// Loads a file containing render settings.
	/// </summary>
	/// <param name="file">File name only of a render settings file, assumes location assets/output/</param>
	void loadRenderSettingsFile(const char* file);

	/// <summary>
	/// Retrieve the render output in a form of 2D image texture. 
	/// </summary>
	/// <returns></returns>
	unsigned int getRenderFinalTexture();

} // Namespace cloudWeatherSim