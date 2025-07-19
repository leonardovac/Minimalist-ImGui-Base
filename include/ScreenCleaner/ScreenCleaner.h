#pragma once
#include "TinyHook/tinyhook.h"


class ScreenCleaner
{
public:
	bool* pDrawingEnabled = nullptr;
	HANDLE eventPresentSkipped = CreateEvent(nullptr, FALSE, FALSE, nullptr);

	TinyHook::IATHook* IAT = nullptr;

	ScreenCleaner() = default;
	explicit ScreenCleaner(bool* pDrawEnabled) : pDrawingEnabled(pDrawEnabled) {}

	bool Init();

	~ScreenCleaner();
};

extern ScreenCleaner screenCleaner;