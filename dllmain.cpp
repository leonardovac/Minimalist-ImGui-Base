#include <thread>

#include "windows.h"
#include "src/base.h"

typedef DWORD(*GetFileVersionInfoSizeA_Type)(LPCSTR lptstrFilename, LPDWORD lpdwHandle);
typedef DWORD(*GetFileVersionInfoSizeW_Type)(LPCWSTR lptstrFilename, LPDWORD lpdwHandle);
typedef DWORD(*GetFileVersionInfoSizeExW_Type)(DWORD dwFlags, LPCWSTR lptstrFilename, LPDWORD lpdwHandle);
typedef BOOL(*GetFileVersionInfoA_Type)(LPCSTR lptstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData);
typedef BOOL(*GetFileVersionInfoW_Type)(LPCWSTR lptstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData);
typedef BOOL(*GetFileVersionInfoExW_Type)(DWORD dwFlags, LPCWSTR lptstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData);
typedef BOOL(*VerQueryValueA_Type)(LPCVOID pBlock, LPCSTR lpSubBlock, LPVOID* lplpBuffer, PUINT puLen);
typedef BOOL(*VerQueryValueW_Type)(LPCVOID pBlock, LPCWSTR lpSubBlock, LPVOID* lplpBuffer, PUINT puLen);

static HMODULE hModule = LoadLibraryW(L"c:\\windows\\system32\\version.dll");

BOOL WINAPI DllMain(const HMODULE hinstDLL, const DWORD fdwReason, LPVOID)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(hinstDLL);
		std::thread(MainThread).detach();
	}
	return TRUE;
}

DWORD GetFileVersionInfoSizeA_Proxy(const LPCSTR lptstrFilename, const LPDWORD lpdwHandle)
{
	const auto original = reinterpret_cast<GetFileVersionInfoSizeA_Type>(GetProcAddress(hModule, "GetFileVersionInfoSizeA"));
	return original(lptstrFilename, lpdwHandle);
}
DWORD GetFileVersionInfoSizeW_Proxy(const LPCWSTR lptstrFilename, const LPDWORD lpdwHandle)
{
	const auto original = reinterpret_cast<GetFileVersionInfoSizeW_Type>(GetProcAddress(hModule, "GetFileVersionInfoSizeW"));
	return original(lptstrFilename, lpdwHandle);
}
DWORD GetFileVersionInfoSizeExW_Proxy(const DWORD dwFlags, const LPCWSTR lptstrFilename, const LPDWORD lpdwHandle)
{
	const auto original = reinterpret_cast<GetFileVersionInfoSizeExW_Type>(GetProcAddress(hModule, "GetFileVersionInfoSizeExW"));
	return original(dwFlags, lptstrFilename, lpdwHandle);
}
BOOL GetFileVersionInfoA_Proxy(const LPCSTR lptstrFilename, const DWORD dwHandle, const DWORD dwLen, const LPVOID lpData)
{
	const auto original = reinterpret_cast<GetFileVersionInfoA_Type>(GetProcAddress(hModule, "GetFileVersionInfoA"));
	return original(lptstrFilename, dwHandle, dwLen, lpData);
}
BOOL GetFileVersionInfoW_Proxy(const LPCWSTR lptstrFilename, const DWORD dwHandle, const DWORD dwLen, const LPVOID lpData)
{
	const auto original = reinterpret_cast<GetFileVersionInfoW_Type>(GetProcAddress(hModule, "GetFileVersionInfoW"));
	return original(lptstrFilename, dwHandle, dwLen, lpData);
}
BOOL GetFileVersionInfoExW_Proxy(const DWORD dwFlags, const LPCWSTR lptstrFilename, const DWORD dwHandle, const DWORD dwLen, const LPVOID lpData)
{
	const auto original = reinterpret_cast<GetFileVersionInfoExW_Type>(GetProcAddress(hModule, "GetFileVersionInfoExW"));
	return original(dwFlags, lptstrFilename, dwHandle, dwLen, lpData);
}
BOOL VerQueryValueA_Proxy(const LPCVOID pBlock, const LPCSTR lpSubBlock, LPVOID* lplpBuffer, const PUINT puLen)
{
	const auto original = reinterpret_cast<VerQueryValueA_Type>(GetProcAddress(hModule, "VerQueryValueA"));
	return original(pBlock, lpSubBlock, lplpBuffer, puLen);
}
BOOL VerQueryValueW_Proxy(const LPCVOID pBlock, const LPCWSTR lpSubBlock, LPVOID* lplpBuffer, const PUINT puLen)
{
	const auto original = reinterpret_cast<VerQueryValueW_Type>(GetProcAddress(hModule, "VerQueryValueW"));
	return original(pBlock, lpSubBlock, lplpBuffer, puLen);
}