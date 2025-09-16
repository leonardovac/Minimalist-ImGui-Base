#include "menu.h"

#include <windows.h>

#include "font_awesome.hpp"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "overlay.h"
#include "roboto_mono.hpp"
#include "components/widgets.h"

namespace
{
	void SetupThemeStyle()
	{
		ImGui::StyleColorsDark();
		ImGuiStyle& style = ImGui::GetStyle();

		//style.WindowPadding = ImVec2(12.0f, 12.0f);
		style.WindowRounding = 0.0f;
		style.WindowBorderSize = 0.0f;
		style.WindowMinSize = ImVec2(20.0f, 20.0f);
		style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
		style.ChildRounding = 0.0f;
		style.ChildBorderSize = 1.0f;
		style.PopupRounding = 0.0f;
		style.PopupBorderSize = 1.0f;
		//style.FramePadding = ImVec2(6.0f, 6.0f);
		style.FramePadding.x = 6.0f;
		style.FrameRounding = 0.0f;
		style.FrameBorderSize = 0.0f;
		style.ItemSpacing = ImVec2(12.0f, 6.0f);
		//style.ItemInnerSpacing = ImVec2(6.0f, 3.0f);
		//style.CellPadding = ImVec2(12.0f, 6.0f);
		style.IndentSpacing = 20.0f;
		style.ColumnsMinSpacing = 6.0f;
		style.ScrollbarSize = 12.0f;
		style.ScrollbarRounding = 0.0f;
		style.GrabMinSize = 12.0f;
		style.GrabRounding = 0.0f;
		style.TabRounding = 0.0f;
		style.TabBorderSize = 0.0f;

		style.Colors[ImGuiCol_Text] = ImVec4(0.80f, 0.80f, 0.83f, 1.00f);
		style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
		style.Colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
		style.Colors[ImGuiCol_ChildBg] = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);
		style.Colors[ImGuiCol_PopupBg] = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);
		style.Colors[ImGuiCol_Border] = ImVec4(0.80f, 0.80f, 0.83f, 0.88f);
		style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.92f, 0.91f, 0.88f, 0.00f);
		style.Colors[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
		style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
		style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.32f, 0.31f, 0.37f, 1.00f);
		style.Colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
		style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.10f, 0.09f, 0.12f, 0.50f);
		style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.14f, 0.17f, 1.00f);
		style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
		style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
		style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.80f, 0.80f, 0.83f, 0.31f);
		style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
		style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.80f, 0.80f, 0.83f, 0.88f);
		style.Colors[ImGuiCol_PopupBg] = ImVec4(0.19f, 0.18f, 0.21f, 1.00f);
		style.Colors[ImGuiCol_CheckMark] = ImVec4(0.80f, 0.80f, 0.83f, 0.31f);
		style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.80f, 0.80f, 0.83f, 0.31f);
		style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.80f, 0.80f, 0.83f, 0.88f);
		style.Colors[ImGuiCol_Button] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
		style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
		style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.32f, 0.31f, 0.37f, 1.00f);
		style.Colors[ImGuiCol_Header] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
		style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
		style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.32f, 0.31f, 0.37f, 1.00f);
		style.Colors[ImGuiCol_Separator] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
		style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
		style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
		style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
		style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.80f, 0.80f, 0.83f, 0.88f);
		style.Colors[ImGuiCol_Tab] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
		style.Colors[ImGuiCol_TabHovered] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
		style.Colors[ImGuiCol_TabActive] = ImVec4(0.15f, 0.14f, 0.17f, 1.00f);
		style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);
		style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.13f, 0.16f, 1.00f);
		style.Colors[ImGuiCol_PlotLines] = ImVec4(0.40f, 0.39f, 0.38f, 0.63f);
		style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
		style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.40f, 0.39f, 0.38f, 0.63f);
		style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
		style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.40f, 0.39f, 0.58f, 0.35f);
		style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(1.00f, 0.98f, 0.95f, 0.73f);
	}
}

void Menu::SetupImGui()
{
	IMGUI_CHECKVERSION();
	lpPrevWndFunc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(Overlay::hWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc)));

	ImGui::CreateContext();
	SetupThemeStyle();
	
	ImGuiIO& io = ImGui::GetIO();

	io.LogFilename = nullptr;
	io.IniFilename = nullptr;

	constexpr float fontSize = 16.5f; // Font size, while the value is ~30% bigger than Default (13.0f), it has (almost) the same size.

	// Main Font
	io.Fonts->AddFontFromMemoryCompressedBase85TTF(roboto_mono.data(), fontSize);

	// Icon Font
	constexpr ImWchar glyphRanges[]{ ICON_MIN_FA, ICON_MAX_FA, 0 };
	ImFontConfig iconsConfig;
	iconsConfig.GlyphOffset = ImVec2(1.f, 1.f);
	iconsConfig.GlyphMinAdvanceX = fontSize;
	iconsConfig.GlyphMaxAdvanceX = fontSize;
	iconsConfig.MergeMode = true;
	iconsConfig.PixelSnapH = true;

	io.Fonts->AddFontFromMemoryCompressedBase85TTF(font_awesome.data(), fontSize, &iconsConfig, glyphRanges);
}

void Menu::CleanupImGui()
{
	SetWindowLongPtr(Overlay::hWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(lpPrevWndFunc));
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void Menu::DrawMenu()
{
	ImGui::SetNextWindowSizeConstraints({ 400, 300 }, { 960, 900 });

	if (ImGui::Begin("Minimalist ImGui Base", &Menu::bOpen, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Hello, world!");
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(ICON_FA_COMPUTER_MOUSE" Hook examples:"); // Can use icons from FontAwesome with their macros outside quotes, ex: ICON "TEXT" ICON "MORE TEXT"
		ImGui::SameLine();
		ImGui::SameLine(0, 1 * ImGui::GetStyle().ItemSpacing.y);
		if (ImGui::Button("MessageBoxA")) 
		{
			MessageBoxA(nullptr, "Lorem ipsum dolor sit amet, consectetur adipiscing elit.", "Lorem ipsum", MB_OK);	// MessageBoxA is hooked in this context (see hooks.h)
		}
		ImGui::SameLine();
		if (ImGui::Button("MessageBoxW"))
		{
			MessageBoxW(nullptr, L"Lorem ipsum dolor sit amet, consectetur adipiscing elit.", L"Lorem ipsum", MB_OK);	// MessageBoxW is hooked in this context (see hooks.h)
		}

		ImGui::CustomBindKey("Open Menu", &Menu::bOpen);
		ImGui::CustomBindKey("Example Keybind", {"example_keybind", &Menu::bExample, ImGuiKey_MouseLeft, Keybinds::KeyBind::HOLD});
		ImGui::Checkbox("Example Checkbox", &Menu::bExample);
		ImGui::Separator();
		const auto& openKey = Keybinds::GetKeyBind(&Menu::bOpen);
		ImGui::Text("Press %s %s to open/close this menu.", openKey->keyIcon.c_str(), openKey->keyName.c_str()); // Icons can also be used inside text with std::format or similar
	}
	ImGui::End();
}
