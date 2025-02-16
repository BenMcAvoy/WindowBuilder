#include "windowbuilder.h"
#include "windowbuilder_imgui.h"

static void Render(Window& window) {
	ImGui::Begin("Hello, world!");
	ImGui::Text("This is some useful text.");
	ImGui::End();
}

int main(void) {
	Window window = WindowBuilder()
		.Name("My Window", "MyWindowClass")
		.Plugin<WindowBuilderImGui>()
		.Size(1280, 720)
		.OnRender(Render)
		.Build();

	window.Show();

	return 0;
}