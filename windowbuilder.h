#pragma once

#pragma comment(lib, "d3d11.lib")

#include <vector>
#include <memory>
#include <functional>
#include <iostream>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <d3d11.h>

class Window;

class WBPlugin {
public:
	virtual ~WBPlugin() = default;

	virtual void OnLoad(Window&) {}
	virtual void OnUnload(Window&) {}

	virtual void PreRender(Window&) {}
	virtual void PostRender(Window&) {}

	virtual void HandleMessage(Window&, UINT, WPARAM, LPARAM) {}
};

class Window {
public:
	/// <summary>
	/// Begin rendering loop
	/// </summary>
	void Show() {
		MSG msg = {};
		while (msg.message != WM_QUIT) {
			if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			else {
				context->ClearRenderTargetView(renderTargetView, this->clearColor);

				for (auto& plugin : plugins)
					plugin->PreRender(*this);
				onRender(*this);
				for (auto& plugin : plugins)
					plugin->PostRender(*this);

				swapChain->Present(0, 0);
			}
		}

		for (auto& plugin : plugins)
			plugin->OnUnload(*this);
	}

	Window(const char* title, const char* className, float clearColor[4], int width, int height, std::function<void(Window&)> onResize = nullptr, std::function<void(Window&)> onClose = nullptr, std::function<void(Window&)> onRender = nullptr, std::vector<std::unique_ptr<WBPlugin>> plugins = {}) {
		this->title = title;
		this->className = className;
		this->plugins = std::move(plugins);

		this->width = width;
		this->height = height;

		memcpy(this->clearColor, clearColor, sizeof(float) * 4);

		if (onResize) this->onResize = onResize;
		if (onClose) this->onClose = onClose;
		if (onRender) this->onRender = onRender;

		this->renderTargetView = nullptr;

		// Register window class
		WNDCLASS wc = {};
		wc.lpfnWndProc = WndProc;
		wc.hInstance = GetModuleHandle(NULL);

#ifdef UNICODE
		wchar_t wClassName[256];
		swprintf(wClassName, 256, L"%hs", className);
		wc.lpszClassName = wClassName;
#else
		wc.lpszClassName = className;
#endif

		RegisterClass(&wc);

		HWND hWnd;
#ifdef UNICODE
		wchar_t wTitle[256];
		swprintf(wTitle, 256, L"%hs", title);
		hWnd = CreateWindow(wClassName, wTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, NULL, NULL, GetModuleHandle(NULL), NULL);
#else
		hWnd = CreateWindow(className, title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, NULL, NULL, GetModuleHandle(NULL), NULL);
#endif

		SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

		this->hWnd = hWnd;
		this->hInstance = GetModuleHandle(NULL);

		// Create DX11 device and swap chain
		DXGI_SWAP_CHAIN_DESC scd = {};
		scd.BufferCount = 1;
		scd.BufferDesc.Width = width;
		scd.BufferDesc.Height = height;
		scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		scd.BufferDesc.RefreshRate.Numerator = 60;
		scd.BufferDesc.RefreshRate.Denominator = 1;
		scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		scd.OutputWindow = hWnd;
		scd.SampleDesc.Count = 1;
		scd.SampleDesc.Quality = 0;
		scd.Windowed = TRUE;

		HRESULT res = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &scd, &swapChain, &device, NULL, &context);
		if (res != S_OK) {
			std::cerr << "Failed to create device and swap chain" << std::endl;

			// Get result string
			char* error = nullptr;
			FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, res, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&error, 0, NULL);
			std::cerr << error << std::endl;

			return;
		}

		// Get back buffer
		ID3D11Texture2D* backBuffer;
		swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
		device->CreateRenderTargetView(backBuffer, NULL, &renderTargetView);
		backBuffer->Release();

		// Show window
		ShowWindow(hWnd, SW_SHOW);
		UpdateWindow(hWnd);
		
		// Set target view
		context->OMSetRenderTargets(1, &renderTargetView, NULL);

		for (auto& plugin : this->plugins) {
			plugin->OnLoad(*this);
		}
	}

public:
	// Hide constructor, copy, and assignment operator
	Window() = delete;
	Window(const Window&) = delete;
	Window& operator=(const Window&) = delete;

	// DX11 stuff
	ID3D11Device* device;
	ID3D11DeviceContext* context;
	IDXGISwapChain* swapChain;
	ID3D11RenderTargetView* renderTargetView;

	// Win32 stuff
	HWND hWnd;
	HINSTANCE hInstance;

	// window properties (updated whenever needed)
	int width = 800;
	int height = 600;
	const char* title = "Window";
	const char* className = "WindowClass";
	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

	std::function<void(Window&)> onResize = [](Window& window) {
		// Resize buffers
		window.context->OMSetRenderTargets(0, 0, 0);
		window.renderTargetView->Release();

		window.swapChain->ResizeBuffers(0, window.width, window.height, DXGI_FORMAT_UNKNOWN, 0);

		ID3D11Texture2D* backBuffer = nullptr;
		window.swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
		window.device->CreateRenderTargetView(backBuffer, NULL, &window.renderTargetView);
		backBuffer->Release();
		};
	std::function<void(Window&)> onClose = [](Window&) {
		PostQuitMessage(0);
		};
	std::function<void(Window&)> onRender = [](Window&) {};

	std::vector<std::unique_ptr<WBPlugin>> plugins = {};

	// window procedure
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
		Window* window = reinterpret_cast<Window*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
		switch (message) {
		case WM_SIZE:
			window->width = LOWORD(lParam);
			window->height = HIWORD(lParam);
			window->onResize(*window);

			break;
		case WM_CLOSE:
			window->onClose(*window);
			break;
		}

		if (window)
		for (auto& plugin : window->plugins) {
			plugin->HandleMessage(*window, message, wParam, lParam);
		}

		return DefWindowProc(hWnd, message, wParam, lParam);
	}
};

class WindowBuilder {
public:
	WindowBuilder() = default;

	/// <summary>
	/// Set window title and class name
	/// </summary>
	/// <param name="title">Title of the window</param>
	/// <param name="className">Class name of the window</param>
	/// <returns></returns>
	WindowBuilder& Name(const char* title, const char* className = "WindowClass") {
		this->title = title;
		return *this;
	}

	/// <summary>
	/// Set window size
	/// </summary>
	/// <param name="width">Width of the window</param>
	/// <param name="height">Height of the window</param>
	/// <returns></returns>
	WindowBuilder& Size(int width, int height) {
		this->width = width;
		this->height = height;
		return *this;
	}

	/// <summary>
	/// Set clear color of the window
	/// </summary>
	/// <param name="r">Red component of the color</param>
	/// <param name="g">Green component of the color</param>
	/// <param name="b">Blue component of the color</param>
	/// <param name="a">Alpha component of the color</param>
	/// <returns></returns>
	WindowBuilder& ClearColor(float r, float g, float b, float a) {
		this->clearColor[0] = r;
		this->clearColor[1] = g;
		this->clearColor[2] = b;
		this->clearColor[3] = a;
		return *this;
	}

	/// <summary>
	/// Set resize callback
	/// </summary>
	/// <param name="onResize">Callback function</param>
	/// <returns></returns>
	WindowBuilder& OnResize(std::function<void(Window&)> onResize) {
		this->onResize = onResize;
		return *this;
	}

	/// <summary>
	/// Set close callback
	/// </summary>
	/// <param name="onClose">Callback function</param>
	/// <returns></returns>
	WindowBuilder& OnClose(std::function<void(Window&)> onClose) {
		this->onClose = onClose;
		return *this;
	}

	/// <summary>
	/// Set render callback
	/// </summary>
	/// <param name="onRender">Callback function</param>
	/// <returns></returns>
	WindowBuilder& OnRender(std::function<void(Window&)> onRender) {
		this->onRender = onRender;
		return *this;
	}

	/// <summary>
	/// Add plugin to the window
	/// </summary>
	/// <typeparam name="T">Type of the plugin</typeparam>
	/// <returns></returns>
	template<typename T>
	WindowBuilder& Plugin() {
		this->plugins.emplace_back(std::make_unique<T>());
		return *this;
	}

	/// <summary>
	/// Build window
	/// </summary>
	/// <returns>A window object</returns>
	Window Build() {
		return Window(title, className, clearColor, width, height, onResize, onClose, onRender, std::move(plugins));
	}

private:
	// window properties
	int width = 800;
	int height = 600;
	const char* title = "Window";
	const char* className = "WindowClass";

	std::function<void(Window&)> onResize = nullptr;
	std::function<void(Window&)> onRender = nullptr;
	std::function<void(Window&)> onClose = nullptr;

	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

	std::vector<std::unique_ptr<WBPlugin>> plugins;
};
