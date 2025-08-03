#pragma once
#include <algorithm>
#include <format>
#include <string>

#include "imgui.h"
#include "../misc/keybinds.h"

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

namespace ImGui
{
#ifdef KEYBINDS_H
    inline bool CustomBindKeyEx(const char* label, Keybinds::KeyBind* keyBind, const bool useCheckbox = false)
    {
		const char* typeText = keyBind->type == Keybinds::KeyBind::TOGGLE ? "[Toggle]" : "[Hold]  ";
		const std::string visibleText = std::format("{} {} Key: {}", typeText, label, keyBind->keyName);
	    const std::string labelText = std::string("##") + label;
		const float itemSize = ImGui::CalcTextSize(visibleText.c_str()).x * 1.02f;	// increasing size, otherwise the last letter will get cropped

	    if (useCheckbox && keyBind->pFunction != nullptr)
	    {
		    Checkbox(visibleText.c_str(), keyBind->pFunction);
	    }
	    else
	    {
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() - 8.f);							// offset hidden text
			ImGui::SetNextItemWidth(itemSize);											// ensuring it can fit our text (It would render max 35 chars otherwise)
		    ImGui::LabelText(labelText.c_str(), "%s", visibleText.c_str());
			if (ImGui::IsItemClicked() && ImGui::GetMouseClickedCount(0) == 2)
			{
				keyBind->ChangeType();
			}
	    }

	    static float distance;
	    distance = std::max(distance, itemSize);
	    ImGui::SameLine(ImGui::GetCursorPosX() + ImGui::GetStyle().ItemSpacing.x + distance);

	    const bool isMouseKey = keyBind->key > 0 && keyBind->key < 7;
	    const std::string buttonIcon = isMouseKey ? ICON_FA_COMPUTER_MOUSE " " : ICON_FA_KEYBOARD;
	    const char* buttonText = keyBind->isWaiting ? " Press any key..." : " Bind Key";
	    const std::string buttonLabel = buttonIcon + buttonText + labelText;

	    if (ImGui::Button(buttonLabel.c_str()))
	    {
		    keyBind->isWaiting = !keyBind->isWaiting;
	    }

	    if (keyBind->key)
	    {
		    ImGui::SameLine();
		    if (ImGui::Button((std::string(ICON_FA_ERASER" Clear") + labelText).c_str()))
		    {
			    keyBind->Update(0, 0);
				distance = 0;
		    }
	    }

	    if (keyBind->isWaiting)
	    {
		    for (std::uint16_t key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_NamedKey_END; key++)
		    {
			    if (key == ImGuiKey_MouseLeft) continue;
			    if (ImGui::IsKeyReleased(static_cast<ImGuiKey>(key)))
			    {
				    keyBind->Update(key, Keybinds::Modifiers::GetModifiers());
					distance = 0;
				    break;
			    }
		    }
	    }

	    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 1);
	    return keyBind->pFunction != nullptr ? *(keyBind->pFunction) : true;
    }

    inline bool CustomBindKey(const char* label, bool* pFunction, const bool useCheckbox = false)
    {
	    if (pFunction == nullptr) return false;

	    if (Keybinds::KeyBind* existingKeyBind = Keybinds::GetKeyBind(pFunction))
	    {
		    return CustomBindKeyEx(label, existingKeyBind, useCheckbox);
	    }

	    Keybinds::kbList.emplace_back(Utils::ToSnakeCase(label), pFunction);
	    return CustomBindKeyEx(label, &Keybinds::kbList.back(), useCheckbox);
    }

    inline bool CustomBindKey(const char* label, const Keybinds::KeyBind& keyBind, const bool useCheckbox = false)
    {
	    if (keyBind.pFunction == nullptr) return false;
	    
	    if (Keybinds::KeyBind* existingKeyBind = Keybinds::GetKeyBind(keyBind.pFunction))
	    {
		    return CustomBindKeyEx(label, existingKeyBind, useCheckbox);
	    }

	    Keybinds::kbList.push_back(keyBind);
	    return CustomBindKeyEx(label, &Keybinds::kbList.back(), useCheckbox);
    }
#endif
}
