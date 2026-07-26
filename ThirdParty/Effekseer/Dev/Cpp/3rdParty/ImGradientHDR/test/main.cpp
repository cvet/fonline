#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>

#include <GLFW/glfw3.h>

static void glfw_error_callback(int error, const char* description)
{
	fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

#include <ImGradientHDR.h>

int main(int, char**)
{
	glfwSetErrorCallback(glfw_error_callback);
	if (!glfwInit())
	{
		return 1;
	}

#if defined(__APPLE__)
	// GL 3.2 + GLSL 150
	const char* glsl_version = "#version 150";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // 3.2+ only
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);		   // Required on Mac
#else
	// GL 3.0 + GLSL 130
	const char* glsl_version = "#version 130";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	// glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
	// glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

	GLFWwindow* window = glfwCreateWindow(1280, 720, "ImGradientHDR", NULL, NULL);
	if (window == NULL)
	{
		return 1;
	}
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	(void)io;

	ImGui::StyleColorsDark();

	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(glsl_version);

	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	int32_t stateID = 10;

	ImGradientHDRState state;
	ImGradientHDRTemporaryState tempState;

	state.AddColorMarker(0.0f, {1.0f, 1.0f, 1.0f}, 1.0f);
	state.AddColorMarker(1.0f, {1.0f, 1.0f, 1.0f}, 1.0f);
	state.AddAlphaMarker(0.0f, 1.0f);
	state.AddAlphaMarker(1.0f, 1.0f);

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		{
			ImGui::Begin("ImGradientHDR");

			bool isMarkerShown = true;
			ImGradientHDR(stateID, state, tempState, isMarkerShown);

			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Gradient");
			}

			if (tempState.selectedMarkerType == ImGradientHDRMarkerType::Color)
			{
				auto selectedColorMarker = state.GetColorMarker(tempState.selectedIndex);
				if (selectedColorMarker != nullptr)
				{
					ImGui::ColorEdit3("Color", selectedColorMarker->Color.data(), ImGuiColorEditFlags_Float);
					ImGui::DragFloat("Intensity", &selectedColorMarker->Intensity, 0.1f, 0.0f, 100.0f, "%f", 1.0f);
				}
			}

			if (tempState.selectedMarkerType == ImGradientHDRMarkerType::Alpha)
			{
				auto selectedAlphaMarker = state.GetAlphaMarker(tempState.selectedIndex);
				if (selectedAlphaMarker != nullptr)
				{
					ImGui::DragFloat("Alpha", &selectedAlphaMarker->Alpha, 0.1f, 0.0f, 1.0f, "%f", 1.0f);
				}
			}

			if (tempState.selectedMarkerType != ImGradientHDRMarkerType::Unknown)
			{
				if (ImGui::Button("Delete"))
				{
					if (tempState.selectedMarkerType == ImGradientHDRMarkerType::Color)
					{
						state.RemoveColorMarker(tempState.selectedIndex);
						tempState = ImGradientHDRTemporaryState{};
					}
					else if (tempState.selectedMarkerType == ImGradientHDRMarkerType::Alpha)
					{
						state.RemoveAlphaMarker(tempState.selectedIndex);
						tempState = ImGradientHDRTemporaryState{};
					}
				}
			}

			ImGui::End();
		}

		ImGui::Render();
		int display_w, display_h;
		glfwGetFramebufferSize(window, &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);
		glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(window);
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}