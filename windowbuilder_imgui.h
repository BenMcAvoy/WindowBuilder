#pragma once

#include "windowbuilder.h"

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

class WindowBuilderImGui : public WBPlugin {
public:
	void OnLoad(Window& window) override {
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;        // Enable Gamepad Controls
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
		io.IniFilename = nullptr;									// Disable .ini file
		ImGui::StyleColorsDark();
		ImGui_ImplWin32_Init(window.hWnd);
		ImGui_ImplDX11_Init(window.device, window.context);
	}

	void OnUnload(Window& window) override {
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}

	void PreRender(Window& window) override {
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
	}

	void PostRender(Window& window) override {
		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	}

	void HandleMessage(Window& window, UINT message, WPARAM wParam, LPARAM lParam) override {
		ImGui_ImplWin32_WndProcHandler(window.hWnd, message, wParam, lParam);
	}
};
