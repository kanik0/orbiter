// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// ========================================================================
// To be linked into all Orbiter addon modules.
// Contains standard module entry point and version information.
// ========================================================================

#ifdef _WIN32
#include <windows.h>
#else
#include "OrbiterPlatform.h"
#endif
#include <fstream>
#include <stdio.h>

#ifdef _WIN32
#define DLLCLBK extern "C" __declspec(dllexport)
#define OAPIFUNC __declspec(dllimport)
#else
#define DLLCLBK extern "C" __attribute__((visibility("default")))
#define OAPIFUNC
#endif

#ifdef _WIN32
BOOL WINAPI DllMain (HINSTANCE hModule,
					 DWORD ul_reason_for_call,
					 LPVOID lpReserved)
{
	OAPIFUNC void InitLib (HINSTANCE hModule);
	typedef void (*DLLEXIT)(HINSTANCE);
	static DLLEXIT DLLExit;

	switch (ul_reason_for_call) {
	case DLL_PROCESS_ATTACH:
		InitLib (hModule);
		DLLExit = (DLLEXIT)GetProcAddress (hModule, "ExitModule");
		if (!DLLExit) DLLExit = (DLLEXIT)GetProcAddress (hModule, "opcDLLExit");
		break;
	case DLL_PROCESS_DETACH:
		if (DLLExit) (*DLLExit)(hModule);
		break;
	}
	return TRUE;
}
#else
// On POSIX platforms, use constructor/destructor attributes instead of DllMain
static void (*s_DLLExit)(HINSTANCE) = nullptr;

__attribute__((constructor))
static void posix_module_init() {
	OAPIFUNC void InitLib (HINSTANCE hModule);
	InitLib(nullptr);

	// Find ExitModule via dlsym on the current module
	void* self = dlopen(nullptr, RTLD_NOW);
	if (self) {
		s_DLLExit = (void(*)(HINSTANCE))dlsym(self, "ExitModule");
		if (!s_DLLExit) s_DLLExit = (void(*)(HINSTANCE))dlsym(self, "opcDLLExit");
		dlclose(self);
	}
}

__attribute__((destructor))
static void posix_module_fini() {
	if (s_DLLExit) s_DLLExit(nullptr);
}
#endif

int oapiGetModuleVersion ()
{
	static int v = 0;
	if (!v) {
		OAPIFUNC int Date2Int (char *date);
		v = Date2Int ((char*)__DATE__);
	}
	return v;
}

DLLCLBK int GetModuleVersion (void)
{
	return oapiGetModuleVersion();
}

void dummy () {}

