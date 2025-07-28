#pragma once
#include <Windows.h>

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_win32.h"
#include "../overlay.h"

namespace Overlay::OpenGL
{
	BOOL WINAPI WglSwapBuffers(HDC hdc);

	inline bool Hook()
	{
		if (const HMODULE opengl32 = GetModuleHandleW(L"opengl32.dll"))
		{
			const auto wglSwapBuffers = reinterpret_cast<void*>(GetProcAddress(opengl32, "wglSwapBuffers"));
			return HooksManager::Setup<InlineHook>(wglSwapBuffers, FUNCTION(WglSwapBuffers));
		}
		return false;
	}

	inline void Cleanup()
	{
		std::thread([]
		{
			HooksManager::Unhook(&WglSwapBuffers);

			DisableRendering();

			ImGui_ImplOpenGL3_Shutdown();
			Menu::CleanupImGui();
		}).detach();
	}

	inline BOOL WINAPI WglSwapBuffers(const HDC hdc)
	{
		[]
		{
			if (!Overlay::bInitialized)
			{
				// Setup Dear ImGui context
				Menu::SetupImGui();
				// Setup Platform/Renderer backends 
				if (!ImGui_ImplWin32_Init(hWindow) || !ImGui_ImplOpenGL3_Init()) return;
				Overlay::bInitialized = true;
			}

			if (!Overlay::bEnabled)
			{
				SetEvent(screenCleaner.eventPresentSkipped);
				return;
			}

			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplWin32_NewFrame();

			Overlay::RenderLogic();

			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		}();

		static const auto original = OriginalFunc(&WglSwapBuffers);
		return original.stdcall<BOOL>(hdc);
	}
}
