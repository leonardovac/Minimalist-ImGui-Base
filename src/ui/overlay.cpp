#include "overlay.h"

#include "../misc/Keybinds.h"
#include "backend/D3D11.h"
#include "backend/D3D12.h"
#include "backend/Discord.h"
#include "backend/OpenGL.h"
#include "backend/Steam.h"

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
	Overlay::CheckGraphicsDriver();

#if HOOK_THIRD_PARTY_OVERLAYS
	if (Overlay::Discord::Init() || Overlay::Steam::Init())
		return true;
#endif

	switch(Overlay::graphicsAPI)
	{
	case GraphicsAPI::UNKNOWN:
		break;
	case GraphicsAPI::D3D9:
		break;
	case GraphicsAPI::D3D11:
		return Overlay::DirectX11::Init();
#ifdef _WIN64
	case GraphicsAPI::D3D12:
		return Overlay::DirectX12::Init();
#endif
	case GraphicsAPI::OpenGL:
		return Overlay::OpenGL::Init();
	case GraphicsAPI::Vulkan:
		break;
	}

	LOG_WARNING("Graphics API {} currently not implemented.", Overlay::graphicsAPI);
	return false;
}
