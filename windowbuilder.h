#pragma once

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "ntdll.lib")

#include <functional>
#include <iostream>
#include <cstring>
#include <cassert>
#include <vector>
#include <memory>
#include <array>
#include <thread>
#include <atomic>
#include <chrono>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <d3d11.h>
#include <dwmapi.h>
#include <winternl.h>
#include <psapi.h>

// Status constants for NT API
#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS 0x00000000
#endif
#ifndef STATUS_INFO_LENGTH_MISMATCH
#define STATUS_INFO_LENGTH_MISMATCH 0xC0000004
#endif

// NT API function declarations for avoiding detection
typedef NTSTATUS(NTAPI* NtQuerySystemInformation_t)(
	SYSTEM_INFORMATION_CLASS SystemInformationClass,
	PVOID SystemInformation,
	ULONG SystemInformationLength,
	PULONG ReturnLength
);

// Structure for system process information
typedef struct _SYSTEM_PROCESS_INFORMATION {
	ULONG NextEntryOffset;
	ULONG NumberOfThreads;
	LARGE_INTEGER Reserved[3];
	LARGE_INTEGER CreateTime;
	LARGE_INTEGER UserTime;
	LARGE_INTEGER KernelTime;
	UNICODE_STRING ImageName;
	KPRIORITY BasePriority;
	HANDLE ProcessId;
	HANDLE InheritedFromProcessId;
} SYSTEM_PROCESS_INFORMATION, * PSYSTEM_PROCESS_INFORMATION;

// Forward declarations
class Window;
class WBPlugin;

// A simple configuration struct for window properties.
struct WindowConfig {
	const char* title = "Window";
	const char* className = "WindowClass";
	std::array<float, 4> clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
	int width = 800;
	int height = 600;
	bool useImmersiveTitlebar = true;
	bool vsync = false; // Pd9ba
	std::function<void(Window&)> onResize = nullptr;
	std::function<void(Window&)> onClose = nullptr;
	std::function<void(Window&)> onRender = nullptr;
	std::vector<std::unique_ptr<WBPlugin>> plugins;
	
	// Overlay/attach configuration
	bool isOverlay = false;
	HWND targetWindow = nullptr;
	const char* targetProcessName = nullptr;
	DWORD targetProcessId = 0;
	bool takeFocus = false;
	bool transparentBackground = true;
};

/// <summary>
/// Base plugin class that can be used to extend the window functionality.
/// </summary>
class WBPlugin {
public:
	/// <summary>
	/// Destructor.
	/// </summary>
	virtual ~WBPlugin() = default;

	/// <summary>
	/// Called when the plugin is loaded.
	/// </summary>
	/// <param name="window">The window instance.</param>
	virtual void OnLoad(Window&) {}

	/// <summary>
	/// Called when the plugin is unloaded.
	/// </summary>
	/// <param name="window">The window instance.</param>
	virtual void OnUnload(Window&) {}

	/// <summary>
	/// Called before the window is rendered.
	/// </summary>
	/// <param name="window">The window instance.</param>
	virtual void PreRender(Window&) {}

	/// <summary>
	/// Called after the window is rendered but before the swap chain is presented.
	/// </summary>
	/// <param name="window">The window instance.</param>
	virtual void PostRender(Window&) {}

	/// <summary>
	/// Called when the window receives a message but previous handlers have not processed it.
	/// </summary>
	/// <param name="window">The window instance.</param>
	/// <param name="msg">The message ID.</param>
	/// <param name="wParam">The WPARAM parameter.</param>
	/// <param name="lParam">The LPARAM parameter.</param>
	virtual void HandleMessage(Window&, UINT, WPARAM, LPARAM) {}
};

/// <summary>
/// A fully built an presentable window.
/// </summary>
class Window {
public:
	// Delete copy operations
	Window(const Window&) = delete;
	Window& operator=(const Window&) = delete;

	// Custom move constructor to handle thread properly
	Window(Window&& other) noexcept
		: device(other.device),
		context(other.context),
		swapChain(other.swapChain),
		renderTargetView(other.renderTargetView),
		hInstance(other.hInstance),
		hWnd(other.hWnd),
		width(other.width),
		height(other.height),
		title(other.title),
		className(other.className),
		clearColor(other.clearColor),
		onResize(std::move(other.onResize)),
		onClose(std::move(other.onClose)),
		onRender(std::move(other.onRender)),
		plugins(std::move(other.plugins)),
		useImmersiveTitlebar(other.useImmersiveTitlebar),
		vsync(other.vsync),
		isOverlay(other.isOverlay),
		targetWindow(other.targetWindow),
		targetProcessName(other.targetProcessName),
		targetProcessId(other.targetProcessId),
		takeFocus(other.takeFocus.load()),
		transparentBackground(other.transparentBackground),
		trackingThread(std::move(other.trackingThread)),
		shouldStopTracking(other.shouldStopTracking.load())
	{
		// Clear the other object
		other.device = nullptr;
		other.context = nullptr;
		other.swapChain = nullptr;
		other.renderTargetView = nullptr;
		other.hWnd = nullptr;
		other.targetWindow = nullptr;
	}

	// Custom move assignment
	Window& operator=(Window&& other) noexcept {
		if (this != &other) {
			// Clean up current resources
			this->~Window();
			
			// Move from other
			new (this) Window(std::move(other));
		}
		return *this;
	}

	~Window() {
		// Stop tracking thread if running
		if (isOverlay && trackingThread.joinable()) {
			shouldStopTracking = true;
			trackingThread.join();
		}

		if (renderTargetView) renderTargetView->Release();
		if (swapChain) swapChain->Release();
		if (context) context->Release();
		if (device) device->Release();
	}

	/// <summary>
	/// Shows the window and enters the message loop.
	/// </summary>
	void Show() {
		MSG msg = {};
		while (msg.message != WM_QUIT) {
			if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			else {
				context->ClearRenderTargetView(renderTargetView, clearColor.data());

				for (auto& plugin : plugins)
					plugin->PreRender(*this);

				if (onRender)
					onRender(*this);

				for (auto& plugin : plugins)
					plugin->PostRender(*this);

				swapChain->Present(vsync ? 1 : 0, 0); // P953f
			}
		}

		// Stop tracking thread if running
		if (isOverlay && trackingThread.joinable()) {
			shouldStopTracking = true;
			trackingThread.join();
		}

		for (auto& plugin : plugins)
			plugin->OnUnload(*this);
	}

	/// <summary>
	/// Sets whether the overlay window should take focus when clicked.
	/// Only applies to overlay windows.
	/// </summary>
	/// <param name="shouldTakeFocus">True to allow focus, false to remain click-through</param>
	void SetTakeFocus(bool shouldTakeFocus) {
		if (!isOverlay) return;
		
		takeFocus = shouldTakeFocus;
		
		// Update window extended styles
		LONG_PTR exStyle = GetWindowLongPtr(hWnd, GWL_EXSTYLE);
		if (shouldTakeFocus) {
			exStyle &= ~WS_EX_TRANSPARENT;
		} else {
			exStyle |= WS_EX_TRANSPARENT;
		}
		SetWindowLongPtr(hWnd, GWL_EXSTYLE, exStyle);
	}

	/// <summary>
	/// Gets whether the overlay window takes focus when clicked.
	/// </summary>
	/// <returns>True if focus is taken, false if click-through</returns>
	bool GetTakeFocus() const {
		return takeFocus;
	}

	explicit Window(WindowConfig config)
		: width(config.width),
		height(config.height),
		title(config.title),
		className(config.className),
		clearColor(config.clearColor),
		onResize(config.onResize ? config.onResize : defaultOnResize),
		onClose(config.onClose ? config.onClose : defaultOnClose),
		onRender(config.onRender ? config.onRender : defaultOnRender),
		plugins(std::move(config.plugins)),
		useImmersiveTitlebar(config.useImmersiveTitlebar),
		vsync(config.vsync), // P953f
		isOverlay(config.isOverlay),
		targetWindow(config.targetWindow),
		targetProcessName(config.targetProcessName),
		targetProcessId(config.targetProcessId),
		takeFocus(config.takeFocus),
		transparentBackground(config.transparentBackground)
	{
		// If overlay mode, try to find target window if not already specified
		if (isOverlay && !targetWindow) {
			targetWindow = FindTargetWindow();
			if (!targetWindow) {
				std::cerr << "Warning: Could not find target window for overlay" << std::endl;
				// Continue with normal window creation if target not found
				isOverlay = false;
			}
		}

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

		// Create window with appropriate styles for overlay or normal window
		HWND hWnd = nullptr;
		DWORD windowStyle = isOverlay ? WS_POPUP : WS_OVERLAPPEDWINDOW;
		DWORD exStyle = 0;
		
		if (isOverlay) {
			exStyle = WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_NOACTIVATE;
			if (!takeFocus) {
				exStyle |= WS_EX_TRANSPARENT;
			}
		}

		// Get target window position and size for overlay
		int x = CW_USEDEFAULT, y = CW_USEDEFAULT;
		if (isOverlay && targetWindow) {
			RECT targetRect;
			if (GetWindowRect(targetWindow, &targetRect)) {
				x = targetRect.left;
				y = targetRect.top;
				width = targetRect.right - targetRect.left;
				height = targetRect.bottom - targetRect.top;
			}
		}

#ifdef UNICODE
		wchar_t wTitle[256];
		swprintf(wTitle, 256, L"%hs", title);
		hWnd = CreateWindowEx(exStyle, wClassName, wTitle, windowStyle,
			x, y, width, height,
			NULL, NULL, GetModuleHandle(NULL), NULL);
#else
		hWnd = CreateWindowEx(exStyle, className, title, windowStyle,
			x, y, width, height,
			NULL, NULL, GetModuleHandle(NULL), NULL);
#endif

		// Associate this Window instance with the HWND
		SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
		this->hWnd = hWnd;
		this->hInstance = GetModuleHandle(NULL);

		// Set up layered window attributes for overlay
		if (isOverlay) {
			// Set transparency
			BYTE alpha = transparentBackground ? 200 : 255; // Semi-transparent background
			SetLayeredWindowAttributes(hWnd, RGB(0, 0, 0), alpha, LWA_ALPHA);
		}

		// Optionally enable an immersive (e.g., dark mode) titlebar
		if (useImmersiveTitlebar && !isOverlay) {
			HKEY hKey = nullptr;
			if (RegOpenKeyEx(HKEY_CURRENT_USER,
#ifdef UNICODE
				L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
#else
				"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
#endif
				0, KEY_READ, &hKey) == ERROR_SUCCESS) {
				DWORD value = 0;
				DWORD size = sizeof(DWORD);
				if (RegQueryValueEx(hKey,
#ifdef UNICODE
					L"AppsUseLightTheme",
#else
					"AppsUseLightTheme",
#endif
					NULL, NULL,
					reinterpret_cast<LPBYTE>(&value), &size) == ERROR_SUCCESS) {
					BOOL dark = value == 0;
					DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE,
						&dark, sizeof(BOOL));
				}
				RegCloseKey(hKey);
			}
		}

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

		HRESULT res = D3D11CreateDeviceAndSwapChain(
			nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
			nullptr, 0, D3D11_SDK_VERSION, &scd,
			&swapChain, &device, nullptr, &context);
		if (res != S_OK) {
			std::cerr << "Failed to create device and swap chain" << std::endl;
			LPSTR errorMsg = nullptr;
			FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
				FORMAT_MESSAGE_IGNORE_INSERTS,
				nullptr, res, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				reinterpret_cast<LPSTR>(&errorMsg), 0, nullptr);
			std::cerr << errorMsg << std::endl;
			LocalFree(errorMsg);
			return;
		}

		// Create render target view
		ID3D11Texture2D* backBuffer = nullptr;
		swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
			reinterpret_cast<void**>(&backBuffer));
		assert(backBuffer != nullptr);
		device->CreateRenderTargetView(backBuffer, nullptr, &renderTargetView);
		backBuffer->Release();

		ShowWindow(hWnd, SW_SHOW);
		UpdateWindow(hWnd);

		context->OMSetRenderTargets(1, &renderTargetView, nullptr);

		// Start tracking thread for overlay mode
		if (isOverlay && targetWindow) {
			shouldStopTracking = false;
			trackingThread = std::thread(&Window::TrackTargetWindow, this);
		}

		// Notify plugins that the window has loaded
		for (auto& plugin : plugins)
			plugin->OnLoad(*this);
	}

	// DX11/Win32 objects
	ID3D11Device* device = nullptr;
	ID3D11DeviceContext* context = nullptr;
	IDXGISwapChain* swapChain = nullptr;
	ID3D11RenderTargetView* renderTargetView = nullptr;
	HINSTANCE hInstance = nullptr;
	HWND hWnd = nullptr;

	// Window properties
	int width = 800;
	int height = 600;
	const char* title = "Window";
	const char* className = "WindowClass";
	std::array<float, 4> clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
	std::function<void(Window&)> onResize = defaultOnResize;
	std::function<void(Window&)> onClose = defaultOnClose;
	std::function<void(Window&)> onRender = defaultOnRender;
	std::vector<std::unique_ptr<WBPlugin>> plugins = {};
	bool useImmersiveTitlebar = false;
	bool vsync = false; // P953f
	
	// Overlay/attach properties
	bool isOverlay = false;
	HWND targetWindow = nullptr;
	const char* targetProcessName = nullptr;
	DWORD targetProcessId = 0;
	std::atomic<bool> takeFocus = false;
	bool transparentBackground = true;
	std::thread trackingThread;
	std::atomic<bool> shouldStopTracking = false;

	// Window procedure
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
		Window* window = reinterpret_cast<Window*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
		if (window) {
			switch (message) {
			case WM_SIZE:
				window->width = LOWORD(lParam);
				window->height = HIWORD(lParam);
				if (window->onResize)
					window->onResize(*window);
				break;
			case WM_CLOSE:
				if (window->onClose)
					window->onClose(*window);
				break;
			}

			for (auto& plugin : window->plugins)
				plugin->HandleMessage(*window, message, wParam, lParam);
		}

		return DefWindowProc(hWnd, message, wParam, lParam);
	}

private:
	// Helper methods for overlay functionality
	HWND FindTargetWindow() {
		if (targetProcessId != 0) {
			return FindWindowByProcessId(targetProcessId);
		}
		else if (targetProcessName) {
			return FindWindowByProcessName(targetProcessName);
		}
		return nullptr;
	}

	HWND FindWindowByProcessId(DWORD processId) {
		struct EnumData {
			DWORD targetPid;
			HWND result;
		};
		
		EnumData data = { processId, nullptr };
		
		EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
			EnumData* data = reinterpret_cast<EnumData*>(lParam);
			DWORD pid;
			GetWindowThreadProcessId(hwnd, &pid);
			if (pid == data->targetPid && IsWindowVisible(hwnd)) {
				data->result = hwnd;
				return FALSE; // Stop enumeration
			}
			return TRUE; // Continue enumeration
		}, reinterpret_cast<LPARAM>(&data));
		
		return data.result;
	}

	HWND FindWindowByProcessName(const char* processName) {
		// Use NT API to avoid detection
		HMODULE ntdll = GetModuleHandleA("ntdll.dll");
		if (!ntdll) return nullptr;

		NtQuerySystemInformation_t NtQuerySystemInformation = 
			reinterpret_cast<NtQuerySystemInformation_t>(GetProcAddress(ntdll, "NtQuerySystemInformation"));
		if (!NtQuerySystemInformation) return nullptr;

		ULONG bufferSize = 0x10000;
		std::vector<BYTE> buffer(bufferSize);

		NTSTATUS status = NtQuerySystemInformation(SystemProcessInformation, 
			buffer.data(), bufferSize, &bufferSize);
		
		if (status == STATUS_INFO_LENGTH_MISMATCH) {
			buffer.resize(bufferSize);
			status = NtQuerySystemInformation(SystemProcessInformation, 
				buffer.data(), bufferSize, &bufferSize);
		}

		if (status != STATUS_SUCCESS) return nullptr;

		PSYSTEM_PROCESS_INFORMATION processInfo = 
			reinterpret_cast<PSYSTEM_PROCESS_INFORMATION>(buffer.data());

		while (processInfo->NextEntryOffset != 0) {
			if (processInfo->ImageName.Buffer) {
				// Convert UNICODE_STRING to char*
				int len = WideCharToMultiByte(CP_UTF8, 0, processInfo->ImageName.Buffer, 
					processInfo->ImageName.Length / sizeof(WCHAR), nullptr, 0, nullptr, nullptr);
				std::vector<char> processNameBuffer(len + 1);
				WideCharToMultiByte(CP_UTF8, 0, processInfo->ImageName.Buffer, 
					processInfo->ImageName.Length / sizeof(WCHAR), 
					processNameBuffer.data(), len, nullptr, nullptr);
				processNameBuffer[len] = '\0';

				if (strcmp(processNameBuffer.data(), processName) == 0) {
					DWORD pid = static_cast<DWORD>(reinterpret_cast<uintptr_t>(processInfo->ProcessId));
					return FindWindowByProcessId(pid);
				}
			}

			processInfo = reinterpret_cast<PSYSTEM_PROCESS_INFORMATION>(
				reinterpret_cast<BYTE*>(processInfo) + processInfo->NextEntryOffset);
		}

		return nullptr;
	}

	void TrackTargetWindow() {
		RECT lastRect = {};
		GetWindowRect(targetWindow, &lastRect);

		while (!shouldStopTracking) {
			std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS

			if (!IsWindow(targetWindow)) {
				// Target window was closed
				PostMessage(hWnd, WM_CLOSE, 0, 0);
				break;
			}

			RECT currentRect;
			if (GetWindowRect(targetWindow, &currentRect)) {
				// Check if position or size changed
				if (memcmp(&lastRect, &currentRect, sizeof(RECT)) != 0) {
					int newWidth = currentRect.right - currentRect.left;
					int newHeight = currentRect.bottom - currentRect.top;

					SetWindowPos(hWnd, HWND_TOPMOST,
						currentRect.left, currentRect.top,
						newWidth, newHeight,
						SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);

					// Update internal size if changed
					if (newWidth != width || newHeight != height) {
						width = newWidth;
						height = newHeight;
						
						// Trigger resize handling
						if (onResize) {
							onResize(*this);
						}
					}

					lastRect = currentRect;
				}
			}
		}
	}

	// Default callback implementations
	static void defaultOnResize(Window& window) {
		if (window.renderTargetView) window.renderTargetView->Release();
		window.swapChain->ResizeBuffers(0, window.width, window.height, DXGI_FORMAT_UNKNOWN, 0);
		ID3D11Texture2D* backBuffer = nullptr;
		window.swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
			reinterpret_cast<void**>(&backBuffer));
		window.device->CreateRenderTargetView(backBuffer, nullptr, &window.renderTargetView);
		backBuffer->Release();
		window.context->OMSetRenderTargets(1, &window.renderTargetView, nullptr);
	}
	static void defaultOnClose(Window&) {
		PostQuitMessage(0);
	}
	static void defaultOnRender(Window&) {}
};

// A builder class for creating a Window.
class WindowBuilder {
public:
	WindowBuilder() {}

	WindowBuilder& Name(const char* title, const char* className = "WindowClass") {
		config.title = title;
		config.className = className;
		return *this;
	}
	WindowBuilder& Size(int width, int height) {
		config.width = width;
		config.height = height;
		return *this;
	}
	WindowBuilder& ClearColor(float r, float g, float b, float a) {
		config.clearColor = { r, g, b, a };
		return *this;
	}
	WindowBuilder& OnResize(std::function<void(Window&)> onResize) {
		config.onResize = onResize;
		return *this;
	}
	WindowBuilder& OnClose(std::function<void(Window&)> onClose) {
		config.onClose = onClose;
		return *this;
	}
	WindowBuilder& OnRender(std::function<void(Window&)> onRender) {
		config.onRender = onRender;
		return *this;
	}
	WindowBuilder& ImmersiveTitlebar(bool useImmersiveTitlebar = true) {
		config.useImmersiveTitlebar = useImmersiveTitlebar;
		return *this;
	}
	WindowBuilder& VSync(bool vsync = true) { // P8b76
		config.vsync = vsync;
		return *this;
	}

	/// <summary>
	/// Configures the window to attach to and overlay on top of a target window by handle.
	/// </summary>
	/// <param name="targetHwnd">Handle to the target window to overlay</param>
	/// <param name="takeFocus">Whether the overlay should take focus when clicked (default: false)</param>
	/// <param name="transparent">Whether the background should be semi-transparent (default: true)</param>
	/// <returns>WindowBuilder reference for chaining</returns>
	WindowBuilder& AttachToWindow(HWND targetHwnd, bool takeFocus = false, bool transparent = true) {
		config.isOverlay = true;
		config.targetWindow = targetHwnd;
		config.takeFocus = takeFocus;
		config.transparentBackground = transparent;
		return *this;
	}

	/// <summary>
	/// Configures the window to attach to and overlay on top of a target process by process ID.
	/// </summary>
	/// <param name="processId">Process ID of the target process</param>
	/// <param name="takeFocus">Whether the overlay should take focus when clicked (default: false)</param>
	/// <param name="transparent">Whether the background should be semi-transparent (default: true)</param>
	/// <returns>WindowBuilder reference for chaining</returns>
	WindowBuilder& AttachToProcess(DWORD processId, bool takeFocus = false, bool transparent = true) {
		config.isOverlay = true;
		config.targetProcessId = processId;
		config.takeFocus = takeFocus;
		config.transparentBackground = transparent;
		return *this;
	}

	/// <summary>
	/// Configures the window to attach to and overlay on top of a target process by process name.
	/// </summary>
	/// <param name="processName">Name of the target process (e.g., "notepad.exe")</param>
	/// <param name="takeFocus">Whether the overlay should take focus when clicked (default: false)</param>
	/// <param name="transparent">Whether the background should be semi-transparent (default: true)</param>
	/// <returns>WindowBuilder reference for chaining</returns>
	WindowBuilder& AttachToProcessName(const char* processName, bool takeFocus = false, bool transparent = true) {
		config.isOverlay = true;
		config.targetProcessName = processName;
		config.takeFocus = takeFocus;
		config.transparentBackground = transparent;
		return *this;
	}

	template<typename T>
	WindowBuilder& Plugin() {
		config.plugins.emplace_back(std::make_unique<T>());
		return *this;
	}

	/// <summary>
	/// Creates a new Window instance with the current configuration.
	/// </summary>
	/// <returns>The new Window instance, heap-allocated with a unique pointer.</returns>
	std::unique_ptr<Window> Build() {
		return std::make_unique<Window>(std::move(config));
	}

private:
	WindowConfig config;
};
