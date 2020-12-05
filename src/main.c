#include <windows.h>
#include <winsock2.h>

#include <fxCG50gdb/emulator.h>
#include <fxCG50gdb/gdb.h>
#include <fxCG50gdb/main.h>
#include <fxCG50gdb/stdio.h>

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
	(void)hinstDLL;
	(void)lpReserved;

	if (fdwReason == DLL_PROCESS_ATTACH) {
		fxCG50gdb_printf("Fake CPU dll loaded.\n");
		real_cpu_dll = LoadLibraryA("CPU73050.real.dll");
		if (real_cpu_dll == NULL) {
			fxCG50gdb_printf("Unable to load CPU73050.real.dll E:%d\n", GetLastError());
			return FALSE;
		}
		fxCG50gdb_printf("CPU73050.real.dll loaded at 0x%p\n", (void*)real_cpu_dll);
	}
	return TRUE;
}

void* __stdcall _DLDriverInfo() {
	real_DLDriver f = real_DLDriverInfo();
	void* r = f();
	fxCG50gdb_printf("Proxy DLDriverInfo f = 0x%p, r = 0x%p\n", f, r);
	return r;
}

void* __stdcall _DLDriverInfoCall() {
	real_DLDriver f = real_DLDriverInfoCall();
	void* r = f();
	fxCG50gdb_printf("Proxy DLDriverInfoCall f = 0x%p, r = 0x%p\n", f, r);

	real_cpu_hijack_break();
	gdb_start();
	return r;
}