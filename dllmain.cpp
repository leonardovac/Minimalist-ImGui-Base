#pragma comment(linker,"/export:GetFileVersionInfoA=c:\\windows\\system32\\version.GetFileVersionInfoA,@1")
#pragma comment(linker,"/export:GetFileVersionInfoByHandle=c:\\windows\\system32\\version.GetFileVersionInfoByHandle,@2")
#pragma comment(linker,"/export:GetFileVersionInfoExA=c:\\windows\\system32\\version.GetFileVersionInfoExA,@3")
#pragma comment(linker,"/export:GetFileVersionInfoExW=c:\\windows\\system32\\version.GetFileVersionInfoExW,@4")
#pragma comment(linker,"/export:GetFileVersionInfoSizeA=c:\\windows\\system32\\version.GetFileVersionInfoSizeA,@5")
#pragma comment(linker,"/export:GetFileVersionInfoSizeExA=c:\\windows\\system32\\version.GetFileVersionInfoSizeExA,@6")
#pragma comment(linker,"/export:GetFileVersionInfoSizeExW=c:\\windows\\system32\\version.GetFileVersionInfoSizeExW,@7")
#pragma comment(linker,"/export:GetFileVersionInfoSizeW=c:\\windows\\system32\\version.GetFileVersionInfoSizeW,@8")
#pragma comment(linker,"/export:GetFileVersionInfoW=c:\\windows\\system32\\version.GetFileVersionInfoW,@9")
#pragma comment(linker,"/export:VerFindFileA=c:\\windows\\system32\\version.VerFindFileA,@10")
#pragma comment(linker,"/export:VerFindFileW=c:\\windows\\system32\\version.VerFindFileW,@11")
#pragma comment(linker,"/export:VerInstallFileA=c:\\windows\\system32\\version.VerInstallFileA,@12")
#pragma comment(linker,"/export:VerInstallFileW=c:\\windows\\system32\\version.VerInstallFileW,@13")
#pragma comment(linker,"/export:VerLanguageNameA=c:\\windows\\system32\\version.VerLanguageNameA,@14")
#pragma comment(linker,"/export:VerLanguageNameW=c:\\windows\\system32\\version.VerLanguageNameW,@15")
#pragma comment(linker,"/export:VerQueryValueA=c:\\windows\\system32\\version.VerQueryValueA,@16")
#pragma comment(linker,"/export:VerQueryValueW=c:\\windows\\system32\\version.VerQueryValueW,@17")

#include <thread>
#include <windows.h>

extern void MainThread();

BOOL WINAPI DllMain(const HMODULE hinstDLL, const DWORD fdwReason, LPVOID)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(hinstDLL);
		std::thread(MainThread).detach();
	}
	return TRUE;
}