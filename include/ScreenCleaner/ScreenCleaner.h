#pragma once
#include <windows.h>

class ScreenCleaner
{
public:
	bool* pDrawingEnabled = nullptr;
	HANDLE eventPresentSkipped = CreateEvent(nullptr, FALSE, FALSE, nullptr);

	ScreenCleaner() = default;
	explicit ScreenCleaner(bool* pDrawEnabled) : pDrawingEnabled(pDrawEnabled) {}

	static bool Init();

	~ScreenCleaner() = default;
};

extern ScreenCleaner screenCleaner;