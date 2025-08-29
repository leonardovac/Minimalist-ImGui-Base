#pragma once
#ifndef KEYBINDS_H
#define KEYBINDS_H
#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>
#include <windows.h>

#include "font_awesome.hpp"
#include "imgui.h"
#include "../ui/menu.h"

namespace Keybinds
{
	namespace Utils
	{
		inline std::string ToSnakeCase(const char* input)
		{
			std::string output = input;

			std::ranges::transform(output, output.begin(), [](const unsigned char c)
				{
					return std::tolower(c);
				});

			std::ranges::replace(output, ' ', '_');

			std::erase_if(output, [](const unsigned char c)
				{
					return !(std::isalnum(c) || c == '_');
				});

			return output;
		}
	}

	namespace Modifiers
	{
		const std::unordered_map<int, std::uint8_t> keys = 
		{
			{ImGuiKey_LeftCtrl, ImGuiMod_Ctrl},
			{ImGuiKey_RightCtrl, ImGuiMod_Ctrl},
			{ImGuiKey_LeftShift, ImGuiMod_Shift},
			{ImGuiKey_RightShift, ImGuiMod_Shift},
			{ImGuiKey_LeftAlt, ImGuiMod_Alt},
			{ImGuiKey_RightAlt, ImGuiMod_Alt},
			{ImGuiKey_LeftSuper, ImGuiMod_Super},
			{ImGuiKey_RightSuper, ImGuiMod_Super},
		};

		inline std::uint16_t GetModifiers()
		{
			return static_cast<std::uint16_t>(ImGui::GetIO().KeyMods);
		}
	}

	inline std::string GetKeyIcon(const int key)
	{
		return key >= ImGuiKey_MouseLeft && key <= ImGuiKey_MouseWheelY ? ICON_FA_COMPUTER_MOUSE : ICON_FA_KEYBOARD;
	}

	inline std::string GetKeyName(const int key, const std::uint16_t modifiers)
	{
		std::string result;

		static const std::unordered_map<int, int> keyNames = {
			{ImGuiKey_Semicolon, VK_OEM_1},
			{ImGuiKey_Slash, VK_OEM_2},
			{ImGuiKey_GraveAccent, VK_OEM_3},
			{ImGuiKey_LeftBracket, VK_OEM_4},
			{ImGuiKey_Backslash, VK_OEM_5},
			{ImGuiKey_RightBracket, VK_OEM_6},
			{ImGuiKey_Apostrophe, VK_OEM_7},
		};

		if (key)
		{
			if (const auto it = keyNames.find(key); it != keyNames.end())
			{
				WCHAR buffer[256];
				if (const int size = GetKeyNameTextW(static_cast<LONG>(MapVirtualKeyW(it->second, MAPVK_VK_TO_VSC)) << 16, buffer, ARRAYSIZE(buffer)))
				{
					result.resize(size + 1); // null-terminator
					WideCharToMultiByte(CP_UTF8, 0, buffer, -1, result.data(), size + 1, nullptr, nullptr);
				}
			}
			else result = ImGui::GetKeyName(static_cast<ImGuiKey>(key));
		}

		if (result.empty()) return "None";

		if (modifiers & ImGuiMod_Alt) result = "Alt+" + result;
		if (modifiers & ImGuiMod_Shift) result = "Shift+" + result;
		if (modifiers & ImGuiMod_Ctrl) result = "Ctrl+" + result;
		if (modifiers & ImGuiMod_Super) result = "Win+" + result;

		return result;
	}

	class KeyBind
	{
	public:
		enum Type : std::uint8_t
		{
			TOGGLE,
			HOLD
		};
		inline static float button_size {};

		std::string		name;
		bool*			pFunction;
		std::uint16_t	key{ 0 };
		Type			type{ TOGGLE };
		std::uint16_t	modifiers{ 0 };

		std::string		keyName{ "NONE" };
		std::string		keyIcon{ ICON_FA_KEYBOARD };
		bool			isWaiting{ false };

		KeyBind(std::string name, bool* function, const std::uint16_t key = 0, const Type type = TOGGLE, const std::uint16_t modifiers = 0) : name(std::move(name)), pFunction(function), key(key), type(type), modifiers(modifiers), keyName(GetKeyName(key, modifiers)), keyIcon(GetKeyIcon(key)) {}

        void ChangeType()
        {
            type = type == TOGGLE ? HOLD : TOGGLE;
        }

		void Check(const std::uint16_t currentModifiers) const
		{
			if (!pFunction) return;

			const std::uint16_t ownModifier = Modifiers::keys.contains(key) ? Modifiers::keys.at(key) : 0;
			const bool isMatchingModifiers = (modifiers | ownModifier) == currentModifiers;
			if (!isMatchingModifiers) return;

			switch (type)
			{
			case TOGGLE:
				if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(key), false))
				{
					*pFunction = !*pFunction;
				}
				break;
			case HOLD:
				*pFunction = ImGui::IsKeyDown(static_cast<ImGuiKey>(key));
				break;
			}
		}

		void Update(const std::uint16_t pressedKey, const std::uint16_t currentModifiers)
		{
			key = pressedKey;
			modifiers = currentModifiers;
			keyName = GetKeyName(key, modifiers);
			keyIcon = GetKeyIcon(pressedKey);
			isWaiting = false;
			button_size = 0;
		}

		void Clear() { Update(0, 0); }
	};

	inline std::vector<KeyBind> bindsList
	{
		{"open_menu", &Menu::bOpen, ImGuiKey_Insert},
	};

	inline KeyBind* GetKeyBind(const bool* function)
	{
		for (KeyBind& keyBind : bindsList)
		{
			if (keyBind.pFunction == function)
			{
				return &keyBind;
			}
		}
		return {};
	}

	inline void CheckKeybinds()
	{
		const std::uint16_t currentModifiers = static_cast<std::uint16_t>(ImGui::GetIO().KeyMods);
		for (KeyBind& keyBind : bindsList)
		{
			keyBind.Check(currentModifiers);
		}
	}
}

namespace ImGui
{
	inline bool CustomBindKey(const char* label, Keybinds::KeyBind* keyBind, const bool useCheckbox = false)
	{
		const std::string labelText = std::string("##") + label;
		const float itemSize = ImGui::CalcTextSize(label).x;

		if (useCheckbox && keyBind->pFunction != nullptr)
		{
			Checkbox(label, keyBind->pFunction);
		}
		else
		{
			ImGui::AlignTextToFramePadding();
			ImGui::SetNextItemWidth(itemSize);
			ImGui::TextUnformatted(label);
			if (ImGui::IsItemClicked() && ImGui::GetMouseClickedCount(0) == 2)
			{
				keyBind->ChangeType();
			}
		}

		const std::string buttonText = keyBind->keyIcon + " " + (keyBind->isWaiting ? "Press any key..." : keyBind->keyName);
		const std::string buttonLabel = buttonText + labelText;
		ImVec2 totalSize{ ImGui::CalcTextSize((buttonText + "  ").c_str()).x + ImGui::GetStyle().ItemSpacing.x, ImGui::GetFrameHeight() };

		if (!keyBind->isWaiting)
		{
			Keybinds::KeyBind::button_size = std::max(Keybinds::KeyBind::button_size, totalSize.x);
			totalSize.x = Keybinds::KeyBind::button_size;
		}

		ImGui::SameLine(ImGui::GetContentRegionAvail().x - totalSize.x + ImGui::GetCursorPosX());

		const ImVec2 pos = ImGui::GetCursorScreenPos();
		constexpr float xButtonSize = 16.0f;

		// Main button
		const ImVec2 mainSize{ totalSize.x - xButtonSize, totalSize.y };
		if (ImGui::InvisibleButton(labelText.c_str(), mainSize))
		{
			keyBind->isWaiting = !keyBind->isWaiting;
		}

		/// Start of custom drawings
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		const ImU32 bgColor = ImGui::IsItemHovered() ? ImGui::GetColorU32(ImGuiCol_FrameBgHovered) : keyBind->isWaiting ? ImGui::GetColorU32(ImGuiCol_FrameBgActive) : ImGui::GetColorU32(ImGuiCol_FrameBg);
		const ImGuiStyle style = ImGui::GetStyle();

		// Draw background
		drawList->AddRectFilled(pos, ImVec2(pos.x + totalSize.x, pos.y + totalSize.y), bgColor, style.FrameRounding);
		// Draw border
		drawList->AddRect(pos, ImVec2(pos.x + totalSize.x, pos.y + totalSize.y), ImGui::GetColorU32(ImGuiCol_Border), style.FrameRounding, 0, style.FrameBorderSize);

		// Draw main text
		const ImVec2 textSize = ImGui::CalcTextSize(buttonText.c_str());
		const ImVec2 textPos{ pos.x + 8.0f, pos.y + (totalSize.y - textSize.y) * 0.5f };
		drawList->AddText(textPos, ImGui::GetColorU32(ImGuiCol_Text), buttonText.c_str());

		if (keyBind->key)
		{
			// Clear button
			ImGui::SameLine(0, 0);
			const ImVec2 xPos = ImGui::GetCursorScreenPos();
			if (ImGui::InvisibleButton((labelText + "_clear").c_str(), ImVec2(xButtonSize, totalSize.y)))
			{
				keyBind->Clear();
			}
			const bool xHovered = ImGui::IsItemHovered();

			// Draw separator line
			drawList->AddLine(ImVec2(pos.x + mainSize.x, pos.y + 2), ImVec2(pos.x + mainSize.x, pos.y + totalSize.y - 2), ImGui::GetColorU32(ImGuiCol_TextDisabled), 1.0f);

			// Draw X
			const ImU32 xColor = xHovered ? IM_COL32(255, 100, 100, 255) : IM_COL32(150, 150, 150, 255);
			const ImVec2 xCenter{ xPos.x + xButtonSize * 0.5f, xPos.y + totalSize.y * 0.5f };
			constexpr float xSize = 4.0f;
			drawList->AddLine(ImVec2(xCenter.x - xSize, xCenter.y - xSize), ImVec2(xCenter.x + xSize, xCenter.y + xSize), xColor, 2.0f);
			drawList->AddLine(ImVec2(xCenter.x + xSize, xCenter.y - xSize), ImVec2(xCenter.x - xSize, xCenter.y + xSize), xColor, 2.0f);
		}
		/// End of custom drawings

		if (keyBind->isWaiting)
		{
			for (std::uint16_t key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_NamedKey_END; key++)
			{
				if (key >= ImGuiKey_ReservedForModCtrl && key <= ImGuiKey_ReservedForModSuper) continue;

				const bool isModifierKey = key >= ImGuiKey_LeftCtrl && key <= ImGuiKey_Menu;
				if ((isModifierKey && ImGui::IsKeyReleased(static_cast<ImGuiKey>(key))) || (!isModifierKey && ImGui::IsKeyPressed(static_cast<ImGuiKey>(key))))
				{
					if (key == ImGuiKey_Escape) keyBind->Clear();
					else keyBind->Update(key, Keybinds::Modifiers::GetModifiers());
					break;
				}
			}
		}
		
		return keyBind->pFunction != nullptr ? *keyBind->pFunction : true;
	}

	inline bool CustomBindKey(const char* label, bool* pFunction, const bool useCheckbox = false)
	{
		if (pFunction == nullptr) return false;

		if (Keybinds::KeyBind* existingKeyBind = Keybinds::GetKeyBind(pFunction))
		{
			return CustomBindKey(label, existingKeyBind, useCheckbox);
		}

		Keybinds::bindsList.emplace_back(Keybinds::Utils::ToSnakeCase(label), pFunction);
		return CustomBindKey(label, &Keybinds::bindsList.back(), useCheckbox);
	}

	inline bool CustomBindKey(const char* label, const Keybinds::KeyBind& keyBind, const bool useCheckbox = false)
	{
		if (keyBind.pFunction == nullptr) return false;

		if (Keybinds::KeyBind* existingKeyBind = Keybinds::GetKeyBind(keyBind.pFunction))
		{
			return CustomBindKey(label, existingKeyBind, useCheckbox);
		}

		Keybinds::bindsList.push_back(keyBind);
		return CustomBindKey(label, &Keybinds::bindsList.back(), useCheckbox);
	}
}
 #endif
