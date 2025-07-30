#pragma once
#include <TinyHook/eathook.h>
#include <TinyHook/iathook.h>
#include <TinyHook/vmthook.h>

namespace TinyHook
{
	template<typename T>
	concept tiny_hook = std::is_same_v<T, EATHook> || std::is_same_v<T, IATHook> || std::is_same_v<T, VMTHook>;

	template<typename T>
	concept at_hook = std::is_same_v<T, EATHook> || std::is_same_v<T, IATHook>;

	template<typename T>
	concept vmt_hook = std::is_same_v<T, VMTHook>;

	template <at_hook Type, typename T>
	[[nodiscard]] std::unique_ptr<Type> Setup(void* module, std::string_view name = {})
	{
		return std::make_unique<Type>(module, name);
	}

	template <at_hook Type, typename T>
	[[nodiscard]] std::unique_ptr<Type> Setup(const char* moduleName)
	{
		return std::make_unique<Type>(moduleName);
	}

	template <at_hook Type, typename T>
	[[nodiscard]] std::unique_ptr<Type> Setup(const wchar_t* moduleName)
	{
		return std::make_unique<Type>(moduleName);
	}

	template <vmt_hook Type, typename T>
	[[nodiscard]] std::unique_ptr<Type> Setup(T* vTable, std::string_view name = {})
	{
		return std::make_unique<Type>(vTable, name);
	}
}
