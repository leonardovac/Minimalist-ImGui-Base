#pragma once
#ifndef KEYBINDS_H
#define KEYBINDS_H

#include <imgui.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <windows.h>

namespace Keybinds
{
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

		std::string		name;
		bool*			function;
		std::uint16_t	key = 0;
		Type			type = TOGGLE;
		std::uint16_t	modifiers = 0;

		std::string		keyName;
		bool			isWaiting = false;

		KeyBind(std::string name, bool* function, const std::uint16_t key = 0, Type	type = TOGGLE, const std::uint16_t modifiers = 0) : name(std::move(name)), function(function), key(key), type(type), modifiers(modifiers), keyName(GetKeyName(key, modifiers)) {}

        void ChangeType()
        {
            type = type == TOGGLE ? HOLD : TOGGLE;
        }

		void Check(const std::uint16_t currentModifiers) const
		{
			if (!function) return;

			const std::uint16_t ownModifier = Modifiers::keys.contains(key) ? Modifiers::keys.at(key) : 0;
			const bool isMatchingModifiers = (modifiers | ownModifier) == currentModifiers;
			if (!isMatchingModifiers) return;

			switch (type)
			{
			case TOGGLE:
				if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(key), false))
				{
					*function = !*function;
				}
				break;
			case HOLD:
				*function = ImGui::IsKeyDown(static_cast<ImGuiKey>(key));
				break;
			}
		}

		void Update(const std::uint16_t pressedKey, const std::uint16_t currentModifiers)
		{
			key = pressedKey;
			modifiers = currentModifiers;
			keyName = GetKeyName(key, modifiers);
			isWaiting = false;
		}
	};

	inline std::vector<KeyBind> kbList
	{
		{"open_menu", &Menu::bOpen, ImGuiKey_Insert},
	};

	inline KeyBind* GetKeyBind(const bool* function)
	{
		for (KeyBind& keyBind : kbList)
		{
			if (keyBind.function == function)
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
 #endif
