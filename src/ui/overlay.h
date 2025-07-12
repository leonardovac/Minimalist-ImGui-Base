#pragma once
#include <imgui.h>
#include <windows.h>
#include <wrl/client.h>

#include "menu.h"
#include "SafetyHook/Wrapper.hpp"

using Microsoft::WRL::ComPtr;

/// WndProc
inline WNDPROC lpPrevWndFunc;
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

inline LRESULT WndProc(const HWND hWnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam)
{
	ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);

	if (Menu::bOpen) // Not really needed
	{
		if (const ImGuiIO& io = ImGui::GetIO(); io.WantCaptureMouse || io.WantCaptureKeyboard) return true;
	}

	return CallWindowProc(lpPrevWndFunc, hWnd, uMsg, wParam, lParam);
}

template<typename T>
void SafeRelease(T*& ptr) {
	if (ptr) {
		ptr->Release();
		ptr = nullptr;
	}
}

enum GraphicsAPI : UINT8
{
	UNSUPPORTED,
	D3D9,
	D3D11,
	D3D12,
	OpenGL,
	Vulkan
};

static const char* GraphicsAPIStrings[] = {
	"UNSUPPORTED",
	"D3D9",
	"D3D11",
	"D3D12",
	"OpenGL",
	"Vulkan"
};

inline const char* GraphicsAPIToString(const GraphicsAPI api)
{
	if (api < std::size(GraphicsAPIStrings)) 
		return GraphicsAPIStrings[api];
	return "UNKNOWN";
}

#if LOGGING_ENABLED
#define FORMATTER fmtquill::formatter
#define FORMAT_TO fmtquill::format_to
#else
#define FORMATTER std::formatter
#define FORMAT_TO std::format_to
#endif

template<>
struct FORMATTER<GraphicsAPI>
{
	static constexpr auto parse(const format_parse_context& ctx) { return ctx.begin(); }

	template<typename FormatContext>
	auto format(const GraphicsAPI& api, FormatContext& ctx) const { return FORMAT_TO(ctx.out(), "{}", GraphicsAPIToString(api)); }
};

namespace Overlay
{
	/// Declarations
	inline bool bInitialized;
	inline bool bEnabled{true};

	inline HWND hWindow;
	inline GraphicsAPI graphicsAPI;

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
	inline bool InitWindow()
	{
		RegisterClassExW(&wndClass);
		hWindow = CreateWindowExW(0L, wndClass.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, nullptr, nullptr, wndClass.hInstance, nullptr);
		return hWindow != nullptr;
	}

	inline bool DeleteWindow()
	{
		DestroyWindow(hWindow);
		UnregisterClass(wndClass.lpszClassName, wndClass.hInstance);
		return hWindow == nullptr;
	}

	inline void CheckGraphicsDriver()
	{
		if (GetModuleHandleW(L"d3d11.dll")) graphicsAPI = D3D11;
#ifdef _WIN64
		else if (GetModuleHandleW(L"d3d12.dll")) graphicsAPI = D3D12;
#endif
		else if (GetModuleHandleW(L"opengl32.dll")) graphicsAPI = OpenGL;
		else if (GetModuleHandleW(L"vulkan-1.dll")) graphicsAPI = Vulkan;
		else graphicsAPI = UNSUPPORTED;
	}
}