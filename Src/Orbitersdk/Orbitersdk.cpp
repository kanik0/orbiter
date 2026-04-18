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
static HMODULE s_selfHandle = nullptr;

// Resolve the handle for THIS module (the dylib that contains
// posix_module_init). Orbiter::LoadModule dlopen()s plugins with
// RTLD_LOCAL, so `dlsym(RTLD_DEFAULT, "InitModule")` can't see a
// plugin's own InitModule — the symbol is confined to the local
// namespace. We need a handle that points at THIS dylib so
// InitLib's GetProcAddress call resolves against the caller.
//
// Strategy: dladdr() on a function pointer we know lives inside this
// dylib tells us the backing file path; dlopen(RTLD_NOLOAD) then
// returns the existing handle without re-loading the file.
static HMODULE resolve_self_handle() {
	Dl_info info{};
	if (!dladdr((void*)&resolve_self_handle, &info) || !info.dli_fname) {
		return nullptr;
	}
	// RTLD_NOLOAD: do not load; return handle iff the file is already mapped.
	return (HMODULE)dlopen(info.dli_fname, RTLD_NOW | RTLD_NOLOAD);
}

__attribute__((constructor))
static void posix_module_init() {
	OAPIFUNC void InitLib (HINSTANCE hModule);
	s_selfHandle = resolve_self_handle();
	InitLib(s_selfHandle);

	if (s_selfHandle) {
		s_DLLExit = (void(*)(HINSTANCE))dlsym(s_selfHandle, "ExitModule");
		if (!s_DLLExit) s_DLLExit = (void(*)(HINSTANCE))dlsym(s_selfHandle, "opcDLLExit");
	}
}

__attribute__((destructor))
static void posix_module_fini() {
	if (s_DLLExit) s_DLLExit(s_selfHandle);
	if (s_selfHandle) dlclose(s_selfHandle);
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

