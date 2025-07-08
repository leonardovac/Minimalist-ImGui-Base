#pragma once
#include <imgui.h>
#include <windows.h>
#include <wrl/client.h>

#include "menu.h"
#include "../misc/keybinds.h"
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

enum GraphicsAPI : UINT8
{
	UNSUPPORTED,
	D3D11,
	D3D12,
	OpenGL,
	Vulkan
};

namespace Overlay
{
	/// Declarations
	inline bool bInitialized;
	inline bool bEnabled{true};

	inline GraphicsAPI graphicsAPI;

	inline WNDCLASSEX wndClass;
	inline HWND hWindow;
	inline HWND hGameWindow;

	// Functions
	void RenderLogic();
	bool TryAllPresentMethods();

	/// Helpers
	inline bool InitWindow()
	{
		wndClass.cbSize = sizeof(WNDCLASSEX);
		wndClass.style = CS_HREDRAW | CS_VREDRAW;
		wndClass.lpfnWndProc = DefWindowProcW;
		wndClass.cbClsExtra = 0;
		wndClass.cbWndExtra = 0;
		wndClass.hInstance = GetModuleHandleW(nullptr);
		wndClass.hIcon = nullptr;
		wndClass.hCursor = nullptr;
		wndClass.hbrBackground = nullptr;
		wndClass.lpszMenuName = nullptr;
		wndClass.lpszClassName = L"Overlay";
		wndClass.hIconSm = nullptr;
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