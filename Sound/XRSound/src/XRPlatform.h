// Copyright (c) Martin Schweiger
// Licensed under the MIT License
//
// XRPlatform — compat shims for Windows-specific C runtime and Win32
// API calls that pepper the original XRSound source. Each shim maps a
// single MS-ism to the matching POSIX (or standard C++) primitive so
// every XRSound .cpp compiles on macOS and Linux without further
// per-site edits.
//
// On Windows this header is a no-op: the original identifiers are
// already defined by the MSVCRT / Win32 SDK, and the #ifdef _WIN32
// guard ensures we don't shadow them.

#ifndef __XRPLATFORM_H
#define __XRPLATFORM_H

#ifndef _WIN32

// Pick up OrbiterPlatform.h's portability shims first — it already
// provides sprintf_s / strcpy_s / strncpy_s / fopen_s on non-Windows.
// Bringing it in here avoids two separate forks of the same shims.
#include "OrbiterPlatform.h"

#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <strings.h>

// --- Debug asserts --------------------------------------------------------
// _ASSERTE is MSVC's debug-build assert; on release it's a no-op. The
// standard <cassert> macro gives identical semantics when NDEBUG tracks
// the build type correctly.
#define _ASSERTE(expr) assert(expr)

// --- Case-insensitive string compare -------------------------------------
#define _stricmp(a, b)      ::strcasecmp((a), (b))
#define _strnicmp(a, b, n)  ::strncasecmp((a), (b), (n))

// Win32 <WinDef.h> integer maxima that XRSound's source references
// (e.g. DefaultSoundGroupPreSteps.cpp:269 uses MAXINT as a "clamp to
// the biggest int" sentinel). Map onto the standard C++ <climits> ones.
#include <climits>
#ifndef MAXINT
	#define MAXINT  INT_MAX
#endif
#ifndef MAXLONG
	#define MAXLONG LONG_MAX
#endif
#ifndef MAXINT32
	#define MAXINT32 INT32_MAX
#endif
#ifndef MAXUINT32
	#define MAXUINT32 UINT32_MAX
#endif

// Win32 shell helper — true iff a file with the given path exists on
// disk. Implemented with stat() so it handles both files and folders
// consistently with Shlwapi's PathFileExists semantics.
#include <sys/stat.h>
inline bool PathFileExists(const char *path) {
	struct stat st;
	return path && ::stat(path, &st) == 0;
}

// GetTickCount64 → monotonic clock in milliseconds since an
// unspecified origin. Good enough for the delta-time callers in
// XRSoundDLL.cpp that compare `now - then` against an interval.
#include <ctime>
#include <cstdint>
inline uint64_t GetTickCount64() {
	struct timespec ts;
	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
	return (uint64_t)ts.tv_sec * 1000ull + (uint64_t)(ts.tv_nsec / 1000000);
}

// sprintf_s / strcpy_s / strncpy_s / fopen_s are provided by the
// canonical OrbiterPlatform.h shims above — redeclaring them would
// trigger -Wredundant-decls and inline-redefinition errors. Nothing
// further needed here.

#endif // !_WIN32

#endif // __XRPLATFORM_H
