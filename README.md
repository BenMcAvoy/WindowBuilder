# WindowBuilder

Extremely simple Windows DX11 helper library for making windows with libraries integrated. 

## Example usage
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
		.Build();

	window.Show();

	return 0;
}
```