#ifndef MAIN_H
#define MAIN_H

#include <windows.h>

BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID);
void* __stdcall __declspec(dllexport) _DLDriverInfo();
void* __stdcall __declspec(dllexport) _DLDriverInfoCall();

#endif
