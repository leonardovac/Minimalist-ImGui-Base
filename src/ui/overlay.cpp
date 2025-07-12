#include "overlay.h"

#include "../misc/Keybinds.h"
#include "backend/D3D11.h"
#include "backend/D3D12.h"
#include "backend/Discord.h"
#include "backend/OpenGL.h"
#include "backend/Steam.h"

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
	if (!Overlay::InitWindow())
		return false;

	Overlay::CheckGraphicsDriver();

	if (Overlay::Discord::Init() || Overlay::Steam::Init())
		return true;

	switch(Overlay::graphicsAPI)
	{
	case GraphicsAPI::D3D9:
		return false;
	case GraphicsAPI::D3D11:
		return Overlay::DirectX11::Init();
#ifdef _WIN64
	case GraphicsAPI::D3D12:
		return Overlay::DirectX12::Init();
#endif
	case GraphicsAPI::OpenGL:
		return Overlay::OpenGL::Init();
	case GraphicsAPI::Vulkan:
	default:
		return false;
	}
}
