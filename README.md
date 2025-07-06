# WindowBuilder

Extremely simple Windows DX11 helper library for making windows with libraries integrated. 
Made simply to speed up the process of creating windows with libraries like ImGui, Dear ImGui, etc when making applications/tools.

## Features

- Simple window creation with DirectX 11 rendering
- Plugin system for easy integration with libraries like ImGui
- **Overlay/Attach functionality** - Create transparent overlay windows that attach to other applications
- Immersive dark mode titlebar support
- VSync control

## Example usage

### Basic Window
```cpp
#include "windowbuilder.h"
#include "windowbuilder_imgui.h"

static void Render() {
	ImGui::Begin("Example");
	ImGui::Text("Hello, world!");
	ImGui::End();
}

int main(void) {
	Window window = WindowBuilder()
		.Name("Example", "ExampleWindowClass")
		.Plugin<WindowBuilderImGui>()
		.OnRender(Render)
		.Size(1280, 720)
		.VSync(true) // Enable vsync
		.Build();

	window.Show();

	return 0;
}
```

### Overlay Window
```cpp
#include "windowbuilder.h"
#include "windowbuilder_imgui.h"

static void RenderOverlay(Window& window) {
	ImGui::Begin("Overlay", nullptr, ImGuiWindowFlags_NoTitleBar);
	ImGui::Text("Overlay Active");
	
	bool takeFocus = window.GetTakeFocus();
	if (ImGui::Checkbox("Take Focus", &takeFocus)) {
		window.SetTakeFocus(takeFocus);
	}
	ImGui::End();
}

int main(void) {
	auto overlayWindow = WindowBuilder()
		.Name("Overlay", "OverlayClass")
		.Plugin<WindowBuilderImGui>()
		.AttachToProcessName("notepad.exe", false, true) // Attach to notepad
		.OnRender(RenderOverlay)
		.Build();

	overlayWindow->Show();
	return 0;
}
```

## Overlay/Attach Features

The WindowBuilder supports creating transparent overlay windows that can attach to other applications:

### Attach Methods
- `AttachToWindow(HWND, takeFocus, transparent)` - Attach to a specific window handle
- `AttachToProcess(DWORD, takeFocus, transparent)` - Attach to a process by process ID  
- `AttachToProcessName(const char*, takeFocus, transparent)` - Attach to a process by name

### Focus Control
- `SetTakeFocus(bool)` - Control whether overlay takes focus when clicked
- `GetTakeFocus()` - Get current focus behavior

### Features
- Transparent, always-on-top overlay rendering
- Automatic position and size synchronization with target window
- Optional click-through behavior (no focus stealing)
- Uses NT APIs to avoid detection by target applications
- Efficient tracking with minimal CPU overhead
