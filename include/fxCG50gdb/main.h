#ifndef MAIN_H
#define MAIN_H

#include <windows.h>

#if defined(_MSC_VER)
#define DLDRIVERINFO DLDriverInfo
#define DLDRIVERINFOCALL DLDriverInfoCall
#else
#define DLDRIVERINFO _DLDriverInfo
#define DLDRIVERINFOCALL _DLDriverInfoCall
#endif

BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID);
__declspec(dllexport) void* __stdcall DLDRIVERINFO();
__declspec(dllexport) void* __stdcall DLDRIVERINFOCALL();

#endif
