#pragma once
#include <Windows.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_opengl3.h>

#include "../overlay.h"
#include "SafetyHook/Wrapper.hpp"

namespace Overlay::OpenGL
{
	BOOL WINAPI WglSwapBuffers(HDC hdc);

	inline bool Init()
	{
		if (const HMODULE opengl32 = GetModuleHandleW(L"opengl32.dll"))
		{
			if (const auto wglSwapBuffers = reinterpret_cast<void*>(GetProcAddress(opengl32, "wglSwapBuffers")))
			{
				HooksManager::Setup<InlineHook>(wglSwapBuffers, FUNCTION(WglSwapBuffers));
				return true;
			}
		}
		return false;
	}

	inline void Cleanup()
	{
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}

	inline BOOL WINAPI WglSwapBuffers(const HDC hdc)
	{
		[&hdc]
		{
			if (!Overlay::bInitialized)
			{
				ImGui::CreateContext();
				Menu::SetupImGuiStyle();
				Overlay::hWindow = WindowFromDC(hdc);
				lpPrevWndFunc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(hWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc)));
				if (!ImGui_ImplWin32_Init(Overlay::hWindow) || !((Overlay::bInitialized = ImGui_ImplOpenGL3_Init()))) return;
			}

			if (!Overlay::bEnabled) return;

			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplWin32_NewFrame();

			Overlay::RenderLogic();

			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		}();

		return HooksManager::GetOriginal(&WglSwapBuffers).unsafe_stdcall<BOOL>(hdc);
	}
}
