// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef __MEMSTAT_H
#define __MEMSTAT_H

#ifdef _WIN32
#include <windows.h>
#else
#include "OrbiterPlatform.h"
#endif
#ifdef _WIN32
#include <psapi.h>
typedef BOOL (CALLBACK *Proc_GetProcessMemoryInfo)(HANDLE,PPROCESS_MEMORY_COUNTERS,DWORD);
#else
typedef void* PPROCESS_MEMORY_COUNTERS;
typedef BOOL (*Proc_GetProcessMemoryInfo)(HANDLE,PPROCESS_MEMORY_COUNTERS,DWORD);
#endif

class MemStat {
public:
    MemStat ();
    ~MemStat ();

    long HeapUsage ();

private:
    static HMODULE hLib;
	static bool bLib;
    HANDLE hProc;
	Proc_GetProcessMemoryInfo pGetProcessMemoryInfo;
    bool active;
};

#endif // !__MEMSTAT_H