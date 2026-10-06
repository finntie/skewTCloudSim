#include "imgui.h"
#include "imgui_impl.h"

#if defined(BEE_PLATFORM_PC) && defined(BEE_GRAPHICS_OPENGL)

#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "core/engine.hpp"
#include "core/device.hpp"
#include "platform/opengl/open_gl.hpp"

bool ImGui_Impl_Init()
{
	const auto window = bee::Engine.Device().GetWindow();
	const bool opengl = ImGui_ImplOpenGL3_Init();
	const bool glfw = ImGui_ImplGlfw_InitForOpenGL(window, true);	
	return glfw && opengl;
}

void ImGui_Impl_Shutdown()
{
	ImGui_ImplGlfw_Shutdown();    
	ImGui_ImplOpenGL3_Shutdown();
}

void ImGui_Impl_NewFrame()
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

void ImGui_Impl_RenderDrawData(ImDrawData*)
{
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

#endif