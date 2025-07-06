#include "windowbuilder.h"
#include "windowbuilder_imgui.h"

static void RenderOverlay(Window& window) {
	// Create a simple overlay UI
	ImGui::Begin("Overlay", nullptr, 
		ImGuiWindowFlags_NoTitleBar | 
		ImGuiWindowFlags_NoResize | 
		ImGuiWindowFlags_NoMove | 
		ImGuiWindowFlags_NoScrollbar | 
		ImGuiWindowFlags_NoBackground);
	
	ImGui::SetWindowPos(ImVec2(10, 10));
	ImGui::SetWindowSize(ImVec2(200, 100));
	
	ImGui::Text("Overlay Active");
	ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
	
	bool takeFocus = window.GetTakeFocus();
	if (ImGui::Checkbox("Take Focus", &takeFocus)) {
		window.SetTakeFocus(takeFocus);
	}
	
	ImGui::End();
}

int main(void) {
	// Example 1: Attach to a specific process name (e.g., notepad.exe)
	auto overlayWindow = WindowBuilder()
		.Name("Overlay Window", "OverlayClass")
		.Plugin<WindowBuilderImGui>()
		.AttachToProcessName("notepad.exe", false, true) // Attach to notepad, no focus, transparent
		.OnRender(RenderOverlay)
		.Build();

	if (overlayWindow) {
		overlayWindow->Show();
	}

	return 0;
}