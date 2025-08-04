#include "overlay.h"

#include "../misc/Keybinds.h"
#include "backend/D3D11.h"
#include "backend/D3D12.h"
#include "backend/D3D9.h"
#include "backend/Discord.h"
#include "backend/OpenGL.h"
#include "backend/Steam.h"
#include "backend/Vulkan.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WndProc(const HWND hWnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam)
{
	ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);

	if (const ImGuiIO& io = ImGui::GetIO(); io.WantCaptureMouse || io.WantCaptureKeyboard) return 1;

	return CallWindowProc(lpPrevWndFunc, hWnd, uMsg, wParam, lParam);
}

void Overlay::RenderLogic()
{
#ifdef KEYBINDS_H
	Keybinds::CheckKeybinds();
#endif

	ImGui::NewFrame();
	pBgDrawList = ImGui::GetBackgroundDrawList();
	//ImGui::GetIO().MouseDrawCursor = Menu::bOpen;
	if (Menu::bOpen) Menu::DrawMenu();
	ImGui::Render();
}

bool Overlay::TryAllPresentMethods()
{
	// Only if we couldn't get it from the window title
	if (graphicsAPI == UNKNOWN) Overlay::CheckGraphicsDriver();

#if HOOK_THIRD_PARTY_OVERLAYS
	if (Overlay::Discord::Hook() || Overlay::Steam::Hook())
		return true;
#endif

	// Some games (most won't) load multiple modules, so we hook all present.
	bool anySuccess = false;
	if (graphicsAPI & GraphicsAPI::D3D9) anySuccess = Overlay::DirectX9::Hook() || anySuccess;
	if (graphicsAPI & GraphicsAPI::D3D11) anySuccess = Overlay::DirectX11::Hook() || anySuccess;
#ifdef D3D12_SUPPORTED
	if (graphicsAPI & GraphicsAPI::D3D12) anySuccess = Overlay::DirectX12::Hook() || anySuccess;
#endif
	if (graphicsAPI & GraphicsAPI::OpenGL) anySuccess = Overlay::OpenGL::Hook() || anySuccess;
#ifdef VULKAN_BACKEND_ENABLED
	if (graphicsAPI & GraphicsAPI::Vulkan) anySuccess = Overlay::Vulkan::Hook() || anySuccess;
#else
	if (graphicsAPI & GraphicsAPI::Vulkan) LOG_WARNING("Vulkan API detected but backend is DISABLED - install Volk library or #include \"volk.h\".");
#endif
	return anySuccess;
}
