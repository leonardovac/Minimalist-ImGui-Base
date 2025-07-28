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

	if (const ImGuiIO& io = ImGui::GetIO(); io.WantCaptureMouse || io.WantCaptureKeyboard) return true;

	return CallWindowProc(lpPrevWndFunc, hWnd, uMsg, wParam, lParam);
}

void Overlay::RenderLogic()
{
#ifdef KEYBINDS_H
	Keybinds::CheckKeybinds();
#endif

	ImGui::NewFrame();
	//ImGui::GetIO().MouseDrawCursor = Menu::bOpen;
	if (Menu::bOpen) Menu::DrawMenu();
	ImGui::Render();
}

bool Overlay::TryAllPresentMethods()
{
	// Only if we couldn't get it from the window title
	if (!graphicsAPI) Overlay::CheckGraphicsDriver();

#if HOOK_THIRD_PARTY_OVERLAYS
	if (Overlay::Discord::Init() || Overlay::Steam::Init())
		return true;
#endif

	// Some games (most won't) load multiple modules, so we hook all present.
	bool anySuccess = false;
	if (graphicsAPI & GraphicsAPI::D3D9) anySuccess = Overlay::DirectX9::Init() || anySuccess;
	if (graphicsAPI & GraphicsAPI::D3D11) anySuccess = Overlay::DirectX11::Init() || anySuccess;
#ifdef _WIN64
	if (graphicsAPI & GraphicsAPI::D3D12) anySuccess = Overlay::DirectX12::Init() || anySuccess;
#endif
	if (graphicsAPI & GraphicsAPI::OpenGL) anySuccess = Overlay::OpenGL::Init() || anySuccess;
	if (graphicsAPI & GraphicsAPI::Vulkan) anySuccess = Overlay::Vulkan::Init() || anySuccess;
	return anySuccess;
}
