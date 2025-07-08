#pragma once
#include <SafetyHook/safetyhook.hpp>

class ScreenCleaner
{
public:
	bool bInitComplete = false;
	bool bEnabled = false;
	bool* pOverlayEnabled = nullptr;

	SafetyHookInline hook;

	ScreenCleaner();
	ScreenCleaner(bool* pDrawEnabled);

	bool Init();
	void Enable();
	void Disable();
	void Toggle();

	~ScreenCleaner();
};

extern ScreenCleaner screenCleaner;