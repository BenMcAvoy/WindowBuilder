#include "windowbuilder.h"
#include "windowbuilder_imgui.h"
#include <iostream>

static void RenderAdvancedOverlay(Window& window) {
	// Create a comprehensive overlay UI
	ImGui::Begin("WindowBuilder Overlay", nullptr, 
		ImGuiWindowFlags_NoCollapse | 
		ImGuiWindowFlags_AlwaysAutoResize);
	
	ImGui::Text("Overlay Status: %s", window.IsOverlay() ? "Active" : "Inactive");
	
	if (window.IsOverlay()) {
		ImGui::Text("Target Window: 0x%p", window.GetTargetWindow());
		
		bool takeFocus = window.GetTakeFocus();
		if (ImGui::Checkbox("Take Focus", &takeFocus)) {
			window.SetTakeFocus(takeFocus);
		}
		ImGui::SameLine();
		ImGui::TextDisabled("(?)");
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("When enabled, overlay can receive mouse clicks.\nWhen disabled, clicks pass through to target window.");
		}
		
		ImGui::Separator();
		ImGui::Text("Performance:");
		ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
		ImGui::Text("Frame Time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);
		
		ImGui::Separator();
		ImGui::Text("Instructions:");
		ImGui::BulletText("This overlay follows the target window");
		ImGui::BulletText("Toggle 'Take Focus' to enable/disable interaction");
		ImGui::BulletText("Overlay is semi-transparent");
		ImGui::BulletText("Close target window to close overlay");
	}
	
	if (ImGui::Button("Exit Overlay")) {
		PostQuitMessage(0);
	}
	
	ImGui::End();
}

int main() {
	std::cout << "WindowBuilder Overlay Example\n";
	std::cout << "==============================\n\n";
	
	// Try to attach to different common processes
	const char* targetProcesses[] = {
		"notepad.exe",
		"explorer.exe", 
		"calculator.exe",
		"cmd.exe"
	};
	
	std::unique_ptr<Window> overlayWindow = nullptr;
	const char* attachedProcess = nullptr;
	
	// Try to find a target process
	for (const char* processName : targetProcesses) {
		std::cout << "Attempting to attach to " << processName << "... ";
		
		auto testWindow = WindowBuilder()
			.Name("Test Overlay", "TestOverlayClass")
			.Plugin<WindowBuilderImGui>()
			.AttachToProcessName(processName, false, true)
			.OnRender(RenderAdvancedOverlay)
			.Build();
			
		if (testWindow && testWindow->IsOverlay() && testWindow->GetTargetWindow()) {
			overlayWindow = std::move(testWindow);
			attachedProcess = processName;
			std::cout << "SUCCESS!\n";
			break;
		} else {
			std::cout << "not found.\n";
		}
	}
	
	if (overlayWindow) {
		std::cout << "\nOverlay attached to " << attachedProcess << "\n";
		std::cout << "Target window handle: 0x" << std::hex << overlayWindow->GetTargetWindow() << std::dec << "\n";
		std::cout << "Focus mode: " << (overlayWindow->GetTakeFocus() ? "Enabled" : "Disabled") << "\n";
		std::cout << "\nStarting overlay...\n\n";
		
		overlayWindow->Show();
	} else {
		std::cout << "\nNo target processes found. Creating fallback overlay on desktop...\n";
		
		// Fallback: create overlay on desktop window
		HWND desktop = GetDesktopWindow();
		auto fallbackWindow = WindowBuilder()
			.Name("Fallback Overlay", "FallbackOverlayClass")
			.Plugin<WindowBuilderImGui>()
			.AttachToWindow(desktop, false, true)
			.OnRender(RenderAdvancedOverlay)
			.Build();
			
		if (fallbackWindow) {
			std::cout << "Fallback overlay created successfully.\n";
			fallbackWindow->Show();
		} else {
			std::cerr << "Failed to create any overlay window.\n";
			return 1;
		}
	}
	
	return 0;
}