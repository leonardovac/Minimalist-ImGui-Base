#pragma once
#include <windows.h>
#include <wrl/client.h>

#include "imgui.h"
#include "../misc/logger.h"
#include "ScreenCleaner/ScreenCleaner.h"

#define HOOK_THIRD_PARTY_OVERLAYS 1
#define USE_VMTHOOK_WHEN_AVAILABLE 0 // For D3D11 or D3D12

using Microsoft::WRL::ComPtr;

LRESULT WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
inline WNDPROC lpPrevWndFunc{nullptr};

template<typename T>
void SafeRelease(T*& ptr) 
{
	if (ptr) 
	{
		ptr->Release();
		ptr = nullptr;
	}
}

enum GraphicsAPI : UINT8
{
	UNKNOWN = 0,
	D3D9 = 1,
	D3D11 = 2,
	D3D12 = 4,
	OpenGL = 8,
	Vulkan = 16
};

inline GraphicsAPI operator|(const GraphicsAPI a, const GraphicsAPI b)
{
	return static_cast<GraphicsAPI>(static_cast<UINT8>(a) | static_cast<UINT8>(b));
}

inline GraphicsAPI& operator|=(GraphicsAPI& a, const GraphicsAPI b)
{
	a = a | b;
	return a;
}

#if LOGGING_ENABLED
inline std::string GetLoadedAPIsString(const GraphicsAPI loadedModules)
{
	std::vector<std::string> modules;

	if (loadedModules & D3D9) modules.emplace_back("D3D9");
	if (loadedModules & D3D11) modules.emplace_back("D3D11");
#ifdef _WIN64
	if (loadedModules & D3D12) modules.emplace_back("D3D12");
#endif
	if (loadedModules & OpenGL) modules.emplace_back("OpenGL");
	if (loadedModules & Vulkan) modules.emplace_back("Vulkan");

	if (modules.empty()) return "UNKNOWN";

	std::string result = modules.front();
	for (size_t i = 1; i < modules.size() - 1; ++i)
	{
		result += ", " + modules[i];
	}
	if (modules.size() > 1) result += " and " + modules.back();
	return result;
}

template<>
struct fmtquill::formatter<GraphicsAPI>
{
	static constexpr auto parse(const format_parse_context& ctx) { return ctx.begin(); }

	template<typename FormatContext>
	auto format(const GraphicsAPI& api, FormatContext& ctx) const { return fmtquill::format_to(ctx.out(), "{}", GetLoadedAPIsString(api)); }
};
#endif

namespace Overlay
{
	/// Declarations
	inline bool bInitialized{false};
	inline bool bEnabled{true};

	inline static ImDrawList* pBgDrawList{nullptr};

	inline GraphicsAPI graphicsAPI{UNKNOWN};
	inline HWND hWindow{nullptr};

	inline const WNDCLASSEX wndClass{
		.cbSize = sizeof(WNDCLASSEX),
		.style = CS_HREDRAW | CS_VREDRAW,
		.lpfnWndProc = DefWindowProcW,
		.cbClsExtra = 0,
		.cbWndExtra = 0,
		.hInstance = GetModuleHandleW(nullptr),
		.hIcon = nullptr,
		.hCursor = nullptr,
		.hbrBackground = nullptr,
		.lpszMenuName = nullptr,
		.lpszClassName = L"Overlay",
		.hIconSm = nullptr
	};

	// Functions
	void RenderLogic();
	bool TryAllPresentMethods();

	/// Helpers
	struct WinGuard
	{
		WinGuard()
		{
			RegisterClassExW(&wndClass);
			handle = CreateWindowExW(0L, wndClass.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, nullptr, nullptr, wndClass.hInstance, nullptr);
		}
		~WinGuard()
		{
			DestroyWindow(handle);
			UnregisterClass(wndClass.lpszClassName, wndClass.hInstance);
		}
		explicit operator bool() const { return handle != nullptr; }
		explicit operator HWND() const { return handle; }
		HWND handle{ nullptr };
	};

	inline void CheckGraphicsDriver()
	{
		if (GetModuleHandleW(L"d3d9.dll")) graphicsAPI |= D3D9;
		if (GetModuleHandleW(L"d3d11.dll")) graphicsAPI |= D3D11;
#ifdef _WIN64
		if (GetModuleHandleW(L"d3d12.dll")) graphicsAPI |= D3D12;
#endif
		if (GetModuleHandleW(L"opengl32.dll")) graphicsAPI |= OpenGL;
		if (GetModuleHandleW(L"vulkan-1.dll")) graphicsAPI |= Vulkan;

		LOG_NOTICE("Detected API(s): {}.", graphicsAPI);
	}

	inline void DisableRendering()
	{
		Overlay::bEnabled = false;
		WaitForSingleObject(screenCleaner.eventPresentSkipped, 50); // Assuming FPS is at least 20, this should wait for at least 1 frame.
	}
}