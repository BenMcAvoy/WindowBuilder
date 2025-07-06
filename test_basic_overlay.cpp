#include "windowbuilder.h"

static void RenderBasicOverlay(Window& window) {
	// Simple rendering - just clear with a semi-transparent red color
	// This will show as a red overlay on the target window
}

int main(void) {
	// Example: Create a basic overlay that attaches to any window (testing purposes)
	// In practice, you would specify a real target process
	auto overlayWindow = WindowBuilder()
		.Name("Basic Overlay", "BasicOverlayClass")
		.Size(400, 300)
		.ClearColor(1.0f, 0.0f, 0.0f, 0.5f) // Semi-transparent red
		.AttachToProcessName("explorer.exe", false, true) // Attach to Windows Explorer
		.OnRender(RenderBasicOverlay)
		.Build();

	if (overlayWindow) {
		// Test focus toggling
		overlayWindow->SetTakeFocus(false); // Start with no focus
		
		// Show a simple message
		std::cout << "Basic overlay created. Focus mode: " 
			<< (overlayWindow->GetTakeFocus() ? "Enabled" : "Disabled") << std::endl;
		
		overlayWindow->Show();
	} else {
		std::cerr << "Failed to create overlay window" << std::endl;
	}

	return 0;
}