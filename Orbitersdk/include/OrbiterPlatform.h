// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// ======================================================================
//                     ORBITER SOFTWARE DEVELOPMENT KIT
// OrbiterPlatform.h
// Platform abstraction layer for cross-platform compilation
// ======================================================================

#ifndef __ORBITERPLATFORM_H
#define __ORBITERPLATFORM_H

#ifdef _WIN32
// =============================================================
// Windows platform
// =============================================================
#include <windows.h>

#else
// =============================================================
// POSIX platforms (macOS, Linux)
// =============================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <climits>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <dirent.h>

// -----------------------------------------------------------
// Basic Windows types
// -----------------------------------------------------------
typedef uint32_t       DWORD;
typedef int32_t        LONG;
typedef int64_t        LONGLONG;
typedef uint16_t       WORD;
typedef unsigned char  BYTE;
typedef int            BOOL;
typedef unsigned int   UINT;
typedef int            INT;
typedef long           HRESULT;
typedef void*          LPVOID;
typedef const char*    LPCSTR;
typedef char*          LPSTR;
typedef const wchar_t* LPCWSTR;
typedef wchar_t*       LPWSTR;
typedef uintptr_t      WPARAM;
typedef intptr_t       LPARAM;
typedef intptr_t       LRESULT;
typedef intptr_t       LONG_PTR;
typedef uintptr_t      ULONG_PTR;
typedef uintptr_t      DWORD_PTR;
typedef intptr_t       INT_PTR;
typedef uintptr_t      UINT_PTR;
typedef int16_t        INT16;
typedef uint16_t       UINT16;
typedef int64_t        INT64;
typedef uint64_t       UINT64;
typedef uint64_t       DWORDLONG;
typedef char*          PSTR;
typedef const char*    PCSTR;
typedef uint8_t        UINT8;

#ifndef MAX_PATH
#define MAX_PATH 260
#endif


#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef NULL
#define NULL nullptr
#endif

// -----------------------------------------------------------
// Handle types (opaque pointers on non-Windows)
// -----------------------------------------------------------
typedef void* HWND;
typedef void* HDC;
typedef void* HINSTANCE;
typedef void* HMODULE;
typedef void* HANDLE;
typedef void* HBITMAP;
typedef void* HFONT;
typedef void* HBRUSH;
typedef void* HPEN;
typedef void* HICON;
typedef void* HCURSOR;
typedef void* HMENU;
typedef void* HKEY;
typedef void* HRGN;
typedef void* HACCEL;

// Callback type stubs
typedef INT_PTR (*DLGPROC)(HWND, UINT, WPARAM, LPARAM);
typedef LRESULT (*WNDPROC)(HWND, UINT, WPARAM, LPARAM);

// -----------------------------------------------------------
// HRESULT helpers
// -----------------------------------------------------------
#define S_OK             ((HRESULT)0L)
#define S_FALSE          ((HRESULT)1L)
#define E_FAIL           ((HRESULT)0x80004005L)
#define E_INVALIDARG     ((HRESULT)0x80070057L)
#define E_OUTOFMEMORY    ((HRESULT)0x8007000EL)
#define SUCCEEDED(hr)    (((HRESULT)(hr)) >= 0)
#define FAILED(hr)       (((HRESULT)(hr)) < 0)

// -----------------------------------------------------------
// Structures
// -----------------------------------------------------------
typedef struct tagRECT {
	LONG left;
	LONG top;
	LONG right;
	LONG bottom;
} RECT;

typedef struct tagPOINT {
	LONG x;
	LONG y;
} POINT;

typedef struct tagSIZE {
	LONG cx;
	LONG cy;
} SIZE;

typedef RECT*  LPRECT;
typedef SIZE*  LPSIZE;
typedef POINT* LPPOINT;

typedef union _LARGE_INTEGER {
	struct {
		DWORD LowPart;
		LONG  HighPart;
	};
	LONGLONG QuadPart;
} LARGE_INTEGER;

// -----------------------------------------------------------
// Color macros
// -----------------------------------------------------------
typedef DWORD COLORREF;
#define RGB(r,g,b) ((COLORREF)(((BYTE)(r)|((WORD)((BYTE)(g))<<8))|(((DWORD)(BYTE)(b))<<16)))
#define GetRValue(rgb) ((BYTE)(rgb))
#define GetGValue(rgb) ((BYTE)(((WORD)(rgb)) >> 8))
#define GetBValue(rgb) ((BYTE)((rgb) >> 16))

// -----------------------------------------------------------
// DLL export/import macros
// -----------------------------------------------------------
#define __declspec(x)
#define DLLEXPORT __attribute__((visibility("default")))
#define DLLIMPORT
#define DLLCLBK extern "C" __attribute__((visibility("default")))
#define WINAPI
#define CALLBACK
#define APIENTRY

// -----------------------------------------------------------
// Dynamic library loading
// -----------------------------------------------------------
inline HMODULE OrbiterLoadLibrary(const char* path) {
	return dlopen(path, RTLD_NOW | RTLD_LOCAL);
}

inline void* OrbiterGetProcAddress(HMODULE mod, const char* name) {
	return dlsym(mod, name);
}

inline BOOL OrbiterFreeLibrary(HMODULE mod) {
	return dlclose(mod) == 0;
}

// -----------------------------------------------------------
// String compatibility
// -----------------------------------------------------------
#define stricmp strcasecmp
#define _stricmp strcasecmp
#define strnicmp strncasecmp
#define _strnicmp strncasecmp
#define _getcwd getcwd
#define _putenv putenv
#define _fullpath(abs, rel, max) realpath(rel, abs)
#define __int64 int64_t
#define lstrlen strlen
#define _snprintf snprintf
#define sprintf_s snprintf
inline int strcpy_s(char* dest, size_t destsz, const char* src) {
	strncpy(dest, src, destsz);
	if (destsz > 0) dest[destsz-1] = '\0';
	return 0;
}

// Win32 window/GDI stubs
inline HWND GetDesktopWindow() { return nullptr; }
inline BOOL GetWindowRect(HWND, RECT*) { return FALSE; }
inline BOOL GetClientRect(HWND, RECT*) { return FALSE; }

// -----------------------------------------------------------
// Path separator
// -----------------------------------------------------------
#define ORBITER_PATH_SEPARATOR '/'
#define ORBITER_PATH_SEPARATOR_STR "/"

// -----------------------------------------------------------
// MessageBox stub (for error reporting)
// -----------------------------------------------------------
#define MB_OK              0x00000000L
#define MB_ICONERROR       0x00000010L
#define MB_ICONWARNING     0x00000030L
#define MB_ICONINFORMATION 0x00000040L
#define MB_YESNO           0x00000004L
#define IDYES              6
#define IDNO               7

inline int MessageBox(HWND, const char* text, const char* caption, UINT) {
	fprintf(stderr, "[%s] %s\n", caption ? caption : "Message", text ? text : "");
	return 0;
}

// -----------------------------------------------------------
// High-resolution timer
// -----------------------------------------------------------
#ifdef __APPLE__
#include <mach/mach_time.h>
inline BOOL QueryPerformanceFrequency(LARGE_INTEGER* freq) {
	mach_timebase_info_data_t info;
	mach_timebase_info(&info);
	// Convert to counts per second
	freq->QuadPart = (1000000000LL * info.denom) / info.numer;
	return TRUE;
}
inline BOOL QueryPerformanceCounter(LARGE_INTEGER* count) {
	count->QuadPart = (LONGLONG)mach_absolute_time();
	return TRUE;
}
#else
#include <time.h>
inline BOOL QueryPerformanceFrequency(LARGE_INTEGER* freq) {
	freq->QuadPart = 1000000000LL; // nanoseconds
	return TRUE;
}
inline BOOL QueryPerformanceCounter(LARGE_INTEGER* count) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	count->QuadPart = (LONGLONG)ts.tv_sec * 1000000000LL + ts.tv_nsec;
	return TRUE;
}
#endif

// -----------------------------------------------------------
// Directory / file stubs
// -----------------------------------------------------------
inline DWORD GetCurrentDirectory(DWORD nBufferLength, char* lpBuffer) {
	if (getcwd(lpBuffer, nBufferLength)) return (DWORD)strlen(lpBuffer);
	return 0;
}

inline BOOL SetCurrentDirectory(const char* path) {
	return chdir(path) == 0;
}

inline void SetEnvironmentVariable(const char* name, const char* value) {
	if (value) setenv(name, value, 1);
	else unsetenv(name);
}

// -----------------------------------------------------------
// Window message constants (stubs for code that references them)
// -----------------------------------------------------------
#define WM_USER            0x0400
#define WM_COMMAND         0x0111
#define WM_CLOSE           0x0010
#define WM_DESTROY         0x0002
#define WM_SIZE            0x0005
#define WM_PAINT           0x000F
#define WM_KEYDOWN         0x0100
#define WM_KEYUP           0x0101
#define WM_MOUSEMOVE       0x0200
#define WM_LBUTTONDOWN     0x0201
#define WM_LBUTTONUP       0x0202
#define WM_RBUTTONDOWN     0x0204
#define WM_RBUTTONUP       0x0205
#define WM_MOUSEWHEEL      0x020A
#define WM_CHAR            0x0102
#define WM_TIMER           0x0113
#define WM_NOTIFY          0x004E
#define WM_INITDIALOG      0x0110

// -----------------------------------------------------------
// Misc Windows macros
// -----------------------------------------------------------
#define LOWORD(l) ((WORD)((DWORD)(l) & 0xffff))
#define HIWORD(l) ((WORD)((DWORD)(l) >> 16))
#define MAKELONG(a, b) ((LONG)(((WORD)(a)) | ((DWORD)((WORD)(b))) << 16))
// Note: min/max macros not defined - use std::min/std::max instead
// (NOMINMAX is defined in the build to prevent Windows.h from defining them)

// -----------------------------------------------------------
// GDI stubs (unused on non-Windows, but referenced in headers)
// -----------------------------------------------------------
inline int SetBkMode(HDC, int) { return 0; }
inline COLORREF SetTextColor(HDC, COLORREF) { return 0; }
inline COLORREF SetBkColor(HDC, COLORREF) { return 0; }
#define TRANSPARENT 1

#endif // _WIN32

// =============================================================
// Cross-platform path separator (available on all platforms)
// =============================================================
#ifdef _WIN32
#ifndef ORBITER_PATH_SEPARATOR
#define ORBITER_PATH_SEPARATOR '\\'
#define ORBITER_PATH_SEPARATOR_STR "\\"
#endif
#endif

#endif // __ORBITERPLATFORM_H
