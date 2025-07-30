#pragma once
#include <TinyHook/eathook.h>
#include <TinyHook/iathook.h>
#include <TinyHook/vmthook.h>

namespace TinyHook
{
	template <typename Type, typename T>
	[[nodiscard]] std::unique_ptr<Type> Setup(void* module, std::string_view name = {})
	{
		return std::make_unique<Type>(module, name);
	}

	template <typename Type, typename T>
	[[nodiscard]] std::unique_ptr<Type> Setup(const char* moduleName)
	{
		return std::make_unique<Type>(moduleName);
	}

	template <typename Type, typename T>
	[[nodiscard]] std::unique_ptr<Type> Setup(const wchar_t* moduleName)
	{
		return std::make_unique<Type>(moduleName);
	}

	template <typename Type, typename T>
	[[nodiscard]] std::unique_ptr<Type> Setup(T* vTable, std::string_view name = {})
	{
		return std::make_unique<Type>(vTable, name);
	}
}
