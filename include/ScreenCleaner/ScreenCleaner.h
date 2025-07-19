#pragma once
#include "VMTHook/iathook.h"


class ScreenCleaner
{
public:
	bool* pDrawingEnabled = nullptr;
	HANDLE eventPresentSkipped = CreateEvent(nullptr, FALSE, FALSE, nullptr);

	HookFramework::IATHook* IAT = nullptr;

	ScreenCleaner() = default;
	explicit ScreenCleaner(bool* pDrawEnabled) : pDrawingEnabled(pDrawEnabled) {}

	bool Init();

	~ScreenCleaner();
};

extern ScreenCleaner screenCleaner;