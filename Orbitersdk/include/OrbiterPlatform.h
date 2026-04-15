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
#include <cerrno>
#include <string>
#include <algorithm>
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
typedef long           LONG;
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
#define FAR
#define PASCAL
#define CDECL

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

// Map Windows DLL loading APIs to POSIX equivalents
#define LoadLibrary(x)     OrbiterLoadLibrary(x)
#define LoadLibraryA(x)    OrbiterLoadLibrary(x)
#define GetProcAddress     OrbiterGetProcAddress
#define FreeLibrary        OrbiterFreeLibrary

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
inline int GetSystemMetrics(int) { return 0; }
inline BOOL GetCursorPos(POINT* p) { if(p) { p->x = 0; p->y = 0; } return FALSE; }
inline BOOL SetCursorPos(int, int) { return FALSE; }
inline BOOL ScreenToClient(HWND, POINT*) { return FALSE; }
inline BOOL ClientToScreen(HWND, POINT*) { return FALSE; }

// Window management stubs
inline HWND GetFocus() { return nullptr; }
inline HWND SetFocus(HWND) { return nullptr; }
inline HWND GetParent(HWND) { return nullptr; }
inline HWND GetDlgItem(HWND, int) { return nullptr; }
inline LONG_PTR GetWindowLongPtr(HWND, int) { return 0; }
inline LONG_PTR SetWindowLongPtr(HWND, int, LONG_PTR) { return 0; }
inline BOOL InvalidateRect(HWND, const RECT*, BOOL) { return FALSE; }
inline BOOL MoveWindow(HWND, int, int, int, int, BOOL) { return FALSE; }
inline BOOL EnableWindow(HWND, BOOL) { return FALSE; }
inline BOOL SetWindowText(HWND, const char*) { return FALSE; }
inline int  GetWindowText(HWND, char*, int) { return 0; }
inline UINT_PTR SetTimer(HWND, UINT_PTR, UINT, void*) { return 0; }
inline BOOL KillTimer(HWND, UINT_PTR) { return FALSE; }
inline BOOL PostMessage(HWND, UINT, WPARAM, LPARAM) { return FALSE; }
inline LRESULT SendMessage(HWND, UINT, WPARAM, LPARAM) { return 0; }
inline LRESULT DefWindowProc(HWND, UINT, WPARAM, LPARAM) { return 0; }
inline HWND SetCapture(HWND) { return nullptr; }
inline BOOL ReleaseCapture() { return FALSE; }
inline BOOL GetUpdateRect(HWND, RECT*, BOOL) { return FALSE; }
inline int GetScrollPos(HWND, int) { return 0; }
inline int SetScrollPos(HWND, int, int, BOOL) { return 0; }
inline BOOL SetScrollRange(HWND, int, int, int, BOOL) { return FALSE; }
inline BOOL SetScrollInfo(HWND, int, void*, BOOL) { return FALSE; }
inline BOOL ScrollWindow(HWND, int, int, const RECT*, const RECT*) { return FALSE; }
inline BOOL UpdateWindow(HWND) { return FALSE; }
inline HWND HtmlHelp(HWND, const char*, UINT, DWORD) { return nullptr; }

// Timer stubs
inline void timeBeginPeriod(UINT) {}
inline void timeEndPeriod(UINT) {}
inline DWORD timeGetTime() { return 0; }

// Registry stubs
#define HKEY_CURRENT_USER nullptr
#define REG_SZ 1
inline LONG RegOpenKeyEx(HKEY, const char*, DWORD, DWORD, HKEY*) { return 1; }
inline LONG RegQueryValueEx(HKEY, const char*, DWORD*, DWORD*, BYTE*, DWORD*) { return 1; }
inline LONG RegCloseKey(HKEY) { return 0; }
#define KEY_READ 0
#define ERROR_SUCCESS 0

// Misc Win32 stubs
inline DWORD GetLastError() { return 0; }
inline void SetLastError(DWORD) {}
inline BOOL DestroyWindow(HWND) { return FALSE; }
inline HCURSOR SetCursor(HCURSOR) { return nullptr; }
inline HCURSOR LoadCursor(HINSTANCE, const char*) { return nullptr; }
inline HICON LoadIcon(HINSTANCE, const char*) { return nullptr; }
inline void InitCommonControls() {}
inline BOOL SystemParametersInfo(UINT, UINT, void*, UINT) { return FALSE; }
inline BOOL GetClassInfo(HINSTANCE, const char*, void*) { return FALSE; }
inline BOOL RegisterClass(const void*) { return FALSE; }
inline BOOL UnregisterClass(const char*, HINSTANCE) { return FALSE; }
inline HWND CreateDialog(HINSTANCE, const char*, HWND, void*) { return nullptr; }
inline int DialogBox(HINSTANCE, const char*, HWND, void*) { return 0; }
inline void RegisterHtmlCtrl(HINSTANCE, bool) {}
#define MAKEINTRESOURCE(i) ((const char*)(uintptr_t)(i))
#define IDC_WAIT nullptr
#define IDC_ARROW nullptr
#define TEXT(x) x
#define SW_MAXIMIZE 3
#define SPI_GETFONTSMOOTHING 0x004A
#define KEY_QUERY_VALUE 0x0001
#define LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR 0
#define LOAD_LIBRARY_SEARCH_DEFAULT_DIRS 0
inline HMODULE LoadLibraryEx(const char* p, HANDLE, DWORD) { return OrbiterLoadLibrary(p); }

// Console stubs
#define ATTACH_PARENT_PROCESS ((DWORD)-1)
inline BOOL AttachConsole(DWORD) { return FALSE; }
inline BOOL AllocConsole() { return FALSE; }
inline BOOL FreeConsole() { return FALSE; }
inline HWND GetConsoleWindow() { return nullptr; }
inline BOOL ShowWindow(HWND, int) { return FALSE; }

// Wide char / code page stubs
typedef wchar_t WCHAR;
typedef WCHAR*  LPWSTR;
typedef const WCHAR* LPCWSTR;

// Additional type stubs
typedef short SHORT;
typedef char  TCHAR;
typedef char* LPTSTR;
typedef const char* LPCTSTR;
typedef struct { SHORT x; SHORT y; } POINTS;
typedef void* LPNMHDR;
#define SW_HIDE 0
#define SW_SHOW 5
#define CP_UTF8 65001
#define SM_CYSCREEN 1
inline int MultiByteToWideChar(UINT, DWORD, LPCSTR, int, LPWSTR, int) { return 0; }

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
#define WM_SETCURSOR       0x0020
#define WM_NCLBUTTONDBLCLK 0x00A3
#define WM_ENTERSIZEMOVE   0x0231
#define WM_EXITSIZEMOVE    0x0232
#define WM_HSCROLL         0x0114
#define WM_VSCROLL         0x0115
#define WM_CTLCOLORSTATIC  0x0138
#define WM_SIZING          0x0214
#define WM_ACTIVATE        0x0006

#define SM_CXSIZEFRAME     32
#define SM_CYSIZEFRAME     33
#define SM_CXFIXEDFRAME    7
#define SM_CYFIXEDFRAME    8
#define SM_CYCAPTION       4

#define SB_LINEUP    0
#define SB_LINEDOWN  1
#define SB_PAGEUP    2
#define SB_PAGEDOWN  3
#define SB_TOP       6
#define SB_BOTTOM    7
#define SB_THUMBTRACK 5
#define SB_LINELEFT  0
#define SB_LINERIGHT 1
#define SB_VERT      1

#define GWLP_USERDATA (-21)
#define GWL_STYLE     (-16)
#define DWLP_USER     (sizeof(LONG_PTR))

#define GWL_EXSTYLE   (-20)
#define WS_DISABLED   0x08000000L
#define BM_GETSTATE   0x00F2
#define SIF_PAGE      0x0002
#define SIF_RANGE     0x0001

#define HH_DISPLAY_TOPIC 0
#define WM_QUIT            0x0012
#define PM_NOREMOVE        0x0000
#define PM_REMOVE          0x0001

// Message structure stub
typedef struct tagMSG {
	HWND   hwnd;
	UINT   message;
	WPARAM wParam;
	LPARAM lParam;
	DWORD  time;
	POINT  pt;
} MSG, *LPMSG;

inline BOOL PeekMessage(MSG*, HWND, UINT, UINT, UINT) { return FALSE; }
inline BOOL GetMessage(MSG*, HWND, UINT, UINT) { return FALSE; }
inline BOOL TranslateMessage(const MSG*) { return FALSE; }
inline LRESULT DispatchMessage(const MSG*) { return 0; }
inline void PostQuitMessage(int) {}
inline BOOL IsDialogMessage(HWND, MSG*) { return FALSE; }

// Resource ID stubs (values don't matter on non-Windows)
#define IDC_STATIC1 1001
#define IDC_STATIC2 1002
#define IDC_STATIC3 1003
#define IDC_STATIC4 1004
#define IDC_STATIC5 1005
#define IDC_STATIC6 1006
#define IDD_DEMOBK  2001
#define IDI_MAIN_ICON 101

#define IDC_STATIC7 1007

// Threading stubs
typedef DWORD (*LPTHREAD_START_ROUTINE)(void*);
inline HANDLE CreateThread(void*, size_t, LPTHREAD_START_ROUTINE, void*, DWORD, DWORD*) { return nullptr; }
inline HANDLE CreateEvent(void*, BOOL, BOOL, const char*) { return nullptr; }
inline BOOL CloseHandle(HANDLE) { return FALSE; }
inline DWORD WaitForSingleObject(HANDLE, DWORD) { return 0; }
inline BOOL PostThreadMessage(DWORD, UINT, WPARAM, LPARAM) { return FALSE; }
inline BOOL IsChild(HWND, HWND) { return FALSE; }
inline int ShowCursor(BOOL) { return 0; }
inline BOOL ClipCursor(const RECT*) { return FALSE; }
#define INFINITE 0xFFFFFFFF

// GDI object stubs
inline void* CreatePen(int, int, DWORD) { return nullptr; }
inline void* CreateSolidBrush(DWORD) { return nullptr; }
inline void* CreateFont(int,int,int,int,int,int,int,int,int,int,int,int,int,const char*) { return nullptr; }
inline void* CreateBrushIndirect(void*) { return nullptr; }
inline void* GetStockObject(int) { return nullptr; }
inline BOOL DeleteObject(void*) { return FALSE; }
inline void* SelectObject(HDC, void*) { return nullptr; }
inline DWORD GetSysColor(int) { return 0; }
inline BOOL TextOut(HDC, int, int, const char*, int) { return FALSE; }
inline BOOL GetTextExtentPoint32(HDC, const char*, int, SIZE*) { return FALSE; }
inline BOOL Rectangle(HDC, int, int, int, int) { return FALSE; }
inline BOOL MoveToEx(HDC, int, int, POINT*) { return FALSE; }
inline BOOL LineTo(HDC, int, int) { return FALSE; }
inline BOOL BitBlt(HDC, int, int, int, int, HDC, int, int, DWORD) { return FALSE; }
inline HDC BeginPaint(HWND, void*) { return nullptr; }
inline BOOL EndPaint(HWND, void*) { return FALSE; }
inline HDC GetDC(HWND) { return nullptr; }
inline int ReleaseDC(HWND, HDC) { return 0; }
inline HDC CreateCompatibleDC(HDC) { return nullptr; }
inline BOOL DeleteDC(HDC) { return FALSE; }
// PAINTSTRUCT, SCROLLINFO, LOGBRUSH defined below as proper structs
#define PS_SOLID  0
#define FW_BOLD   700
#define FW_NORMAL 400
#define ANSI_CHARSET 0
#define OUT_DEFAULT_PRECIS 0
#define CLIP_DEFAULT_PRECIS 0
#define DEFAULT_QUALITY 0
#define FF_SWISS 0
#define BS_SOLID 0
#define SRCCOPY 0
#define CS_HREDRAW 1
#define CS_VREDRAW 2
#define COLOR_3DSHADOW 0
#define COLOR_3DFACE 0
#define LTGRAY_BRUSH 0
#define NULL_BRUSH 0
#define WHITE_BRUSH 0
#define BLACK_BRUSH 0
#define GRAY_BRUSH 0
#define NULL_PEN 0
#define WHITE_PEN 0

// DirectInput type stubs
typedef struct { char tszProductName[260]; } DIDEVICEINSTANCE;
typedef struct { DWORD dwOfs; DWORD dwData; DWORD dwTimeStamp; DWORD dwSequence; } DIDEVICEOBJECTDATA;
typedef struct { LONG lX; LONG lY; LONG lZ; LONG lRx; LONG lRy; LONG lRz; LONG rglSlider[2]; } DIJOYSTATE2;
#define DIERR_NOTACQUIRED  0x8007001EL
#define DIERR_INPUTLOST    0x8007001FL

// Common Control types
typedef void* HTREEITEM;
typedef struct { HWND hwndFrom; UINT_PTR idFrom; UINT code; } NMHDR;

// More Win32 API stubs
#define _strdup strdup
inline int fopen_s(FILE** f, const char* name, const char* mode) {
	*f = fopen(name, mode);
	return *f ? 0 : errno;
}

// D3D render state and primitive type constants
#define D3DRENDERSTATE_ALPHABLENDENABLE 0
#define D3DRENDERSTATE_AMBIENT 0
#define D3DPT_TRIANGLESTRIP 0
#define D3DFVF_VERTEX 0
#define D3DRGBA(r,g,b,a) (0)

// Additional GDI types
typedef BYTE* LPBYTE;
typedef void* HGDIOBJ;
typedef struct { LONG bmWidth; LONG bmHeight; } BITMAP;
typedef struct { DWORD dwSize; DWORD dwFlags; DWORD dwFourCC; DWORD dwRGBBitCount;
	DWORD dwRBitMask; DWORD dwGBitMask; DWORD dwBBitMask; DWORD dwRGBAlphaBitMask; } DDPIXELFORMAT;
typedef struct { DWORD dwSize; DWORD dwFlags; DWORD dwFillColor; } DDBLTFX;
typedef struct { POINT ptReserved; POINT ptMaxSize; POINT ptMaxPosition; POINT ptMinTrackSize; POINT ptMaxTrackSize; } MINMAXINFO;
typedef struct { UINT lbStyle; COLORREF lbColor; ULONG_PTR lbHatch; } LOGBRUSH;
typedef struct { DWORD cbSize; UINT fMask; int nMin; int nMax; UINT nPage; int nPos; int nTrackPos; } SCROLLINFO;
typedef struct { HDC hdc; BOOL fErase; RECT rcPaint; } PAINTSTRUCT;

// Win32 constants
#define WA_INACTIVE 0
#define WM_GETMINMAXINFO 0x0024
#define WM_POWERBROADCAST 0x0218
#define WM_NCHITTEST 0x0084
#define WM_SYSCOMMAND 0x0112
#define PBT_APMQUERYSUSPEND 0
#define PBT_APMRESUMESUSPEND 7
#define SC_MONITORPOWER 0xF170
#define HTCLIENT 1
#define IDM_EXIT 0
#define IDC_SCROLLBAR1 0
#define IDC_OPT_PAGELIST 0
#define SB_CTL 2
#define SIF_POS 4
#define _chdir chdir

// TreeView stubs
typedef struct { UINT mask; HTREEITEM hItem; UINT state; UINT stateMask; char* pszText; int cchTextMax; int iImage; int iSelectedImage; int cChildren; LPARAM lParam; } TVITEM;
#define TVIF_HANDLE 0x0010
#define TVIF_TEXT 0x0001
#define TVIF_CHILDREN 0x0040
#define TVE_EXPAND 2
#define TVE_COLLAPSE 1
inline HTREEITEM TreeView_GetRoot(HWND) { return nullptr; }
inline BOOL TreeView_Expand(HWND, HTREEITEM, UINT) { return FALSE; }
inline BOOL TreeView_GetItem(HWND, TVITEM*) { return FALSE; }
inline HTREEITEM TreeView_GetChild(HWND, HTREEITEM) { return nullptr; }
inline HTREEITEM TreeView_GetNextSibling(HWND, HTREEITEM) { return nullptr; }
inline BOOL TreeView_SelectItem(HWND, HTREEITEM) { return FALSE; }
inline HTREEITEM TreeView_InsertItem(HWND, void*) { return nullptr; }

// TreeView notification / insert types
typedef struct { NMHDR hdr; TVITEM itemOld; TVITEM itemNew; POINT ptDrag; } NM_TREEVIEW;
typedef struct { HTREEITEM hParent; HTREEITEM hInsertAfter; TVITEM item; } TV_INSERTSTRUCT;
#define TVIF_PARAM   0x0004
#define TVI_LAST     ((HTREEITEM)(uintptr_t)-0x0FFFF)
#define TVN_SELCHANGED (-402)
#define SB_THUMBPOSITION 4
#define ETDT_ENABLE  0x0002

// Additional Win32 stubs
inline BOOL GetScrollInfo(HWND, int, SCROLLINFO*) { return FALSE; }
inline HWND CreateDialogParam(HINSTANCE, const char*, HWND, DLGPROC, LPARAM) { return nullptr; }
#define SPI_SETFONTSMOOTHING 0x004B
#define SPIF_SENDCHANGE 0x0002
#define IDC_IMG 0

// Button/checkbox message constants
#define BM_SETCHECK 0x00F1
#define BST_CHECKED 1
#define BST_UNCHECKED 0

// Resource IDs (non-Windows: unused, just need to compile)
#define IDD_OPTIONS_VISUAL 0
#define IDC_OPT_VIS_CLOUD 0
#define IDC_OPT_VIS_CSHADOW 0
#define IDC_OPT_VIS_HAZE 0
#define IDC_OPT_VIS_FOG 0

inline void EnableThemeDialogTexture(HWND, DWORD) {}

// D3DPT_TRIANGLELIST for Baseobj.cpp render functions
#ifndef D3DPT_TRIANGLELIST
#define D3DPT_TRIANGLELIST 4
#endif

inline BOOL GetObject(HBITMAP, int, void*) { return FALSE; }
inline DWORD GetWindowThreadProcessId(HWND, DWORD*) { return 0; }

// ConsoleManager stub (used in Orbiter.cpp)
namespace ConsoleManager {
	inline bool IsConsoleExclusive() { return false; }
	inline void ShowConsole(bool) {}
}

// iequal is defined in Util.cpp, declared in Util.h

// Process stubs
#define _execl execl

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
