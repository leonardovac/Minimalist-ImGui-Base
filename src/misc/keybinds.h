#pragma once
#ifndef KEYBINDS_H
#define KEYBINDS_H
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

			std::ranges::transform(output, output.begin(), [](unsigned char c)
				{
					return std::tolower(c);
				});

			std::ranges::replace(output, ' ', '_');

			std::erase_if(output, [](unsigned char c)
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
		inline static float gui_spacing {};

		std::string		name;
		bool*			pFunction;
		std::uint16_t	key = 0;
		Type			type = TOGGLE;
		std::uint16_t	modifiers = 0;

		std::string		keyName;
		std::string		keyIcon;
		bool			isWaiting = false;

		KeyBind(std::string name, bool* function, const std::uint16_t key = 0, Type	type = TOGGLE, const std::uint16_t modifiers = 0) : name(std::move(name)), pFunction(function), key(key), type(type), modifiers(modifiers), keyName(GetKeyName(key, modifiers)) {}

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
			isWaiting = false;
			gui_spacing = 0;
		}

		void Clear() { Update(0, 0); }
	};

	inline std::vector<KeyBind> kbList
	{
		{"open_menu", &Menu::bOpen, ImGuiKey_Insert},
	};

	inline KeyBind* GetKeyBind(const bool* function)
	{
		for (KeyBind& keyBind : kbList)
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
		for (KeyBind& keyBind : kbList)
		{
			keyBind.Check(currentModifiers);
		}
	}
}

namespace ImGui
{
	inline bool CustomBindKey(const char* label, Keybinds::KeyBind* keyBind, const bool useCheckbox = false)
	{
		const char* typeText = useCheckbox ? "" : keyBind->type == Keybinds::KeyBind::TOGGLE ? "[T] " : "[H] ";
		const std::string visibleText = std::format("{}{} Key: {}", typeText, label, keyBind->keyName);
		const std::string labelText = std::string("##") + label;
		const float itemSize = ImGui::CalcTextSize(visibleText.c_str()).x;

		if (useCheckbox && keyBind->pFunction != nullptr)
		{
			Checkbox(visibleText.c_str(), keyBind->pFunction);
		}
		else
		{
			ImGui::AlignTextToFramePadding();
			ImGui::SetNextItemWidth(itemSize);
			ImGui::TextUnformatted(visibleText.c_str());
			if (ImGui::IsItemClicked() && ImGui::GetMouseClickedCount(0) == 2)
			{
				keyBind->ChangeType();
			}
		}

		Keybinds::KeyBind::gui_spacing = std::max(Keybinds::KeyBind::gui_spacing, itemSize);
		ImGui::SameLine(ImGui::GetCursorPosX() + ImGui::GetStyle().ItemSpacing.x + Keybinds::KeyBind::gui_spacing, 1 * ImGui::GetStyle().ItemSpacing.x);

		keyBind->keyIcon = keyBind->key >= ImGuiKey_MouseLeft && keyBind->key <= ImGuiKey_MouseWheelY ? ICON_FA_COMPUTER_MOUSE : ICON_FA_KEYBOARD;
		const char* buttonText = keyBind->isWaiting ? "Press any key..." : "Bind Key";
		const std::string buttonLabel = keyBind->keyIcon + " " + buttonText + labelText;

		if (ImGui::Button(buttonLabel.c_str()))
		{
			keyBind->isWaiting = !keyBind->isWaiting;
		}

		if (keyBind->key)
		{
			ImGui::SameLine();
			if (ImGui::Button((std::string(ICON_FA_ERASER" Clear") + labelText).c_str()))
			{
				keyBind->Clear();
			}
		}

		if (keyBind->isWaiting)
		{
			for (std::uint16_t key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_NamedKey_END; key++)
			{
				if (key >= ImGuiKey_ReservedForModCtrl && key <= ImGuiKey_ReservedForModSuper) continue;

				const bool isModifierKey = key >= ImGuiKey_LeftCtrl && key <= ImGuiKey_Menu;
				if ((isModifierKey && ImGui::IsKeyReleased(static_cast<ImGuiKey>(key))) || (!isModifierKey && ImGui::IsKeyPressed(static_cast<ImGuiKey>(key))))
				{
					keyBind->Update(key, Keybinds::Modifiers::GetModifiers());
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

		Keybinds::kbList.emplace_back(Keybinds::Utils::ToSnakeCase(label), pFunction);
		return CustomBindKey(label, &Keybinds::kbList.back(), useCheckbox);
	}

	inline bool CustomBindKey(const char* label, const Keybinds::KeyBind& keyBind, const bool useCheckbox = false)
	{
		if (keyBind.pFunction == nullptr) return false;

		if (Keybinds::KeyBind* existingKeyBind = Keybinds::GetKeyBind(keyBind.pFunction))
		{
			return CustomBindKey(label, existingKeyBind, useCheckbox);
		}

		Keybinds::kbList.push_back(keyBind);
		return CustomBindKey(label, &Keybinds::kbList.back(), useCheckbox);
	}
}
 #endif
