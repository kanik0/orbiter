// Stub resource IDs for non-Windows platforms
// These resource IDs are used by Win32 dialog functions.
// On non-Windows they compile but are non-functional since all
// Win32 dialog functions are stubbed to no-ops.

#ifndef __RESOURCE_STUB_H
#define __RESOURCE_STUB_H

#ifndef _WIN32

// Button/checkbox message constants
#ifndef BM_SETCHECK
#define BM_SETCHECK 0x00F1
#endif
#ifndef BST_CHECKED
#define BST_CHECKED 1
#endif
#ifndef BST_UNCHECKED
#define BST_UNCHECKED 0
#endif
#ifndef CB_SETCURSEL
#define CB_SETCURSEL 0x014E
#endif
#ifndef CB_ADDSTRING
#define CB_ADDSTRING 0x0143
#endif
#ifndef CB_FINDSTRINGEXACT
#define CB_FINDSTRINGEXACT 0x0158
#endif
#ifndef CB_GETCURSEL
#define CB_GETCURSEL 0x0147
#endif
#ifndef CB_GETLBTEXT
#define CB_GETLBTEXT 0x0148
#endif
#ifndef CB_RESETCONTENT
#define CB_RESETCONTENT 0x014B
#endif
#ifndef LB_ADDSTRING
#define LB_ADDSTRING 0x0180
#endif
#ifndef LB_GETCURSEL
#define LB_GETCURSEL 0x0188
#endif
#ifndef LB_GETTEXT
#define LB_GETTEXT 0x0189
#endif

// Stub: all Win32 dialog resource IDs default to 0
// These are only used in SendDlgItemMessage calls which are no-ops
#define IDC_OPT_CRD 0
#define IDC_OPT_CRD_BASE 0
#define IDC_OPT_CRD_CELBODY 0
#define IDC_OPT_CRD_NEGATIVE 0
#define IDC_OPT_CRD_OPACITY 0
#define IDC_OPT_CRD_SCALE 0
#define IDC_OPT_CRD_VESSEL 0
#define IDC_OPT_CSP_BGBRIGHTNESS 0
#define IDC_OPT_CSP_BKGIMAGE 0
#define IDC_OPT_CSP_ENABLEBKGMAP 0
#define IDC_OPT_CSP_ENABLESTARMAP 0
#define IDC_OPT_CSP_ENABLESTARPIX 0
#define IDC_OPT_CSP_STARMAGHI 0
#define IDC_OPT_CSP_STARMAGHISPIN 0
#define IDC_OPT_CSP_STARMAGLO 0
#define IDC_OPT_CSP_STARMAGLOSPIN 0
#define IDC_OPT_CSP_STARMAPEXP 0
#define IDC_OPT_CSP_STARMAPIMAGE 0
#define IDC_OPT_CSP_STARMAPLIN 0
#define IDC_OPT_CSP_STARMINBRT 0
#define IDC_OPT_CSP_STARMINBRTSPIN 0
#define IDC_OPT_JOY_DEAD 0
#define IDC_OPT_JOY_DEVICE 0
#define IDC_OPT_JOY_INIT 0
#define IDC_OPT_JOY_SAT 0
#define IDC_OPT_JOY_STATIC1 0
#define IDC_OPT_JOY_STATIC2 0
#define IDC_OPT_JOY_STATIC3 0
#define IDC_OPT_JOY_STATIC4 0
#define IDC_OPT_JOY_THROTTLE 0
#define IDC_OPT_MFD_INTERVAL 0
#define IDC_OPT_MFD_INTERVALSPIN 0
#define IDC_OPT_MFD_SIZE 0
#define IDC_OPT_MFD_SIZESPIN 0
#define IDC_OPT_MFD_TRANSP 0
#define IDC_OPT_MFD_VCTEXSIZE 0
#define IDC_OPT_MKR 0
#define IDC_OPT_MKR_BASE 0
#define IDC_OPT_MKR_BEACON 0
#define IDC_OPT_MKR_CELBODY 0
#define IDC_OPT_MKR_FEATUREBODY 0
#define IDC_OPT_MKR_FEATURELIST 0
#define IDC_OPT_MKR_FEATURES 0
#define IDC_OPT_MKR_VESSEL 0
#define IDC_OPT_PAGELIST 0
#define IDC_OPT_PANEL_SCALE 0
#define IDC_OPT_PANEL_SCALESPIN 0
#define IDC_OPT_PANEL_SCROLLSPEED 0
#define IDC_OPT_PANEL_SCROLLSPEEDSPIN 0
#define IDC_OPT_PHYS_COMPLEXGRAV 0
#define IDC_OPT_PHYS_DISTMASS 0
#define IDC_OPT_PHYS_RPRESSURE 0
#define IDC_OPT_PHYS_WIND 0
#define IDC_OPT_PLN 0
#define IDC_OPT_PLN_CELGRID 0
#define IDC_OPT_PLN_CNSTBND 0
#define IDC_OPT_PLN_CNSTLABEL 0
#define IDC_OPT_PLN_CNSTLABEL_FULL 0
#define IDC_OPT_PLN_CNSTLABEL_SHORT 0
#define IDC_OPT_PLN_CNSTPATTERN 0
#define IDC_OPT_PLN_ECLGRID 0
#define IDC_OPT_PLN_EQU 0
#define IDC_OPT_PLN_GALGRID 0
#define IDC_OPT_PLN_HRZGRID 0
#define IDC_OPT_PLN_MARKER 0
#define IDC_OPT_PLN_MKRLIST 0
#define IDC_OPT_UI_MOUSEFOCUSMODE 0
#define IDC_OPT_VEC 0
#define IDC_OPT_VEC_DRAG 0
#define IDC_OPT_VEC_LIFT 0
#define IDC_OPT_VEC_LINSCL 0
#define IDC_OPT_VEC_LOGSCL 0
#define IDC_OPT_VEC_OPACITY 0
#define IDC_OPT_VEC_SCALE 0
#define IDC_OPT_VEC_SIDEFORCE 0
#define IDC_OPT_VEC_THRUST 0
#define IDC_OPT_VEC_TORQUE 0
#define IDC_OPT_VEC_TOTAL 0
#define IDC_OPT_VEC_WEIGHT 0
#define IDC_OPT_VESSEL_COMPLEXMODEL 0
#define IDC_OPT_VESSEL_DAMAGE 0
#define IDC_OPT_VESSEL_FUELLIMIT 0
#define IDC_OPT_VESSEL_PADFUEL 0
#define IDC_OPT_VHELP_CRD 0
#define IDC_OPT_VHELP_MKR 0
#define IDC_OPT_VHELP_PLN 0
#define IDC_OPT_VHELP_VEC 0
#define IDC_OPT_VIS_AMBIENT 0
#define IDC_OPT_VIS_CLOUD 0
#define IDC_OPT_VIS_CSHADOW 0
#define IDC_OPT_VIS_ELEV 0
#define IDC_OPT_VIS_ELEVMODE 0
#define IDC_OPT_VIS_FOG 0
#define IDC_OPT_VIS_HAZE 0
#define IDC_OPT_VIS_LIGHTS 0
#define IDC_OPT_VIS_LOCALLIGHT 0
#define IDC_OPT_VIS_LTLEVEL 0
#define IDC_OPT_VIS_MAXLEVEL 0
#define IDC_OPT_VIS_PARTICLE 0
#define IDC_OPT_VIS_REENTRY 0
#define IDC_OPT_VIS_REFWATER 0
#define IDC_OPT_VIS_RIPPLE 0
#define IDC_OPT_VIS_SHADOW 0
#define IDC_OPT_VIS_SPECULAR 0
#define IDC_OPT_VIS_VSHADOW 0

#define IDD_OPTIONS_BODYFORCE 0
#define IDD_OPTIONS_CELSPHERE 0
#define IDD_OPTIONS_FRAMEAXES 0
#define IDD_OPTIONS_INSTRUMENT 0
#define IDD_OPTIONS_JOYSTICK 0
#define IDD_OPTIONS_LABELS 0
#define IDD_OPTIONS_PHYSICS 0
#define IDD_OPTIONS_PLANETARIUM 0
#define IDD_OPTIONS_UI 0
#define IDD_OPTIONS_VESSEL 0
#define IDD_OPTIONS_VISHELPER 0
#define IDD_OPTIONS_VISUAL 0

inline LRESULT SendDlgItemMessage(HWND, int, UINT, WPARAM, LPARAM) { return 0; }
inline HMODULE GetModuleHandle(const char*) { return nullptr; }
inline HANDLE LoadImage(HINSTANCE, const char*, UINT, int, int, UINT) { return nullptr; }
#define IMAGE_BITMAP 0
#define LR_CREATEDIBSECTION 0
#define LR_LOADFROMFILE 0

// Button notifications
#define BM_GETCHECK 0x00F0
#define BN_CLICKED 0

// Text alignment
#define TA_CENTER 6
#define TA_RIGHT  2
#define TA_LEFT   0

// MAKEWPARAM macro
#ifndef MAKEWPARAM
#define MAKEWPARAM(l, h) ((WPARAM)(DWORD)MAKELONG(l, h))
#endif

// DirectDraw constants
#define DDCKEY_SRCBLT 0x00000008

// Process stubs
inline HANDLE GetCurrentProcess() { return nullptr; }
// Win32's GetModuleFileName fills the output buffer with the module's file
// path. The POSIX stub can't recover the real path (caller doesn't pass a
// dlopen handle), but it MUST null-terminate the buffer: callers like
// OrbiterAPI.cpp:InitLib print "Module %s" from uninitialised stack memory
// otherwise, producing garbage log lines such as "Module 0C�k ...".
inline DWORD GetModuleFileName(HMODULE, char* buf, DWORD sz) {
	if (buf && sz > 0) buf[0] = '\0';
	return 0;
}
inline BOOL GetModuleFileNameExA(HANDLE, HMODULE, char* buf, DWORD sz) {
	if (buf && sz > 0) buf[0] = '\0';
	return FALSE;
}
inline BOOL EnumProcessModules(HANDLE, HMODULE*, DWORD, DWORD*) { return FALSE; }

// Memory / Module info stubs
typedef struct { DWORD cb; DWORD PageFaultCount; size_t WorkingSetSize; } PROCESS_MEMORY_COUNTERS;
typedef struct { void* lpBaseOfDll; DWORD SizeOfImage; void* EntryPoint; } MODULEINFO;
inline BOOL GetModuleInformation(HANDLE, HMODULE, MODULEINFO*, DWORD) { return FALSE; }
#define PROCESS_QUERY_INFORMATION 0x0400
#define PROCESS_VM_READ           0x0010
inline HANDLE OpenProcess(DWORD, BOOL, DWORD) { return nullptr; }

// Version info stubs
inline DWORD GetFileVersionInfoSize(const char*, DWORD*) { return 0; }
inline BOOL GetFileVersionInfo(const char*, DWORD, DWORD, void*) { return FALSE; }
inline BOOL VerQueryValueA(const void*, const char*, void**, UINT*) { return FALSE; }

// Shell stubs
inline int SHCreateDirectoryEx(HWND, const char*, void*) { return 0; }

// GDI text (SetTextAlign now in OrbiterPlatform.h)

// Window styles
#define WS_OVERLAPPED   0x00000000L
#define WS_POPUP        0x80000000L
#define WS_VISIBLE      0x10000000L
#define WS_CAPTION      0x00C00000L
#define WS_SYSMENU      0x00080000L
#define WS_EX_TOPMOST   0x00000008L
#define CW_USEDEFAULT   ((int)0x80000000)

// Combobox messages
#define CBN_SELCHANGE   1
#define EN_CHANGE       0x0300

// MSVC-specific functions
#define _fseeki64 fseeko
inline int strncpy_s(char* dst, size_t dstsz, const char* src, size_t cnt) {
	size_t n = cnt < dstsz ? cnt : dstsz - 1;
	strncpy(dst, src, n);
	if (dstsz > 0) dst[n] = '\0';
	return 0;
}

// COM stubs (WIC imaging not available on non-Windows)
#define CLSCTX_INPROC_SERVER 0
#define CLSID_WICImagingFactory nullptr
#define IID_PPV_ARGS(x) nullptr, (void**)x
inline HRESULT CoCreateInstance(void*, void*, DWORD, ...) { return E_FAIL; }
inline HRESULT CoInitialize(void*) { return S_OK; }
inline void CoUninitialize() {}

// Inline window creation stub
inline HWND CreateWindowEx(DWORD, const char*, const char*, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, void*) { return nullptr; }
inline HWND CreateWindow(const char* cls, const char* title, DWORD style, int x, int y, int w, int h, HWND parent, HMENU menu, HINSTANCE inst, void* param) {
	return CreateWindowEx(0, cls, title, style, x, y, w, h, parent, menu, inst, param);
}

// Output debug
inline void OutputDebugString(const char*) {}
inline BOOL StretchBlt(HDC,int,int,int,int,HDC,int,int,int,int,DWORD) { return FALSE; }
inline DWORD GetDlgCtrlID(HWND) { return 0; }
inline DWORD GetProcessId(HANDLE) { return 0; }

// Resource loading
typedef void* HRSRC;
typedef void* HGLOBAL;
inline HRSRC FindResource(HMODULE, const char*, const char*) { return nullptr; }
inline HGLOBAL LoadResource(HMODULE, HRSRC) { return nullptr; }
inline void* LockResource(HGLOBAL) { return nullptr; }

// Bitmap info
#ifndef __BITMAPINFOHEADER_DEFINED
typedef struct { DWORD biSize; LONG biWidth; LONG biHeight; WORD biPlanes; WORD biBitCount; DWORD biCompression; } BITMAPINFOHEADER;
typedef struct { BITMAPINFOHEADER bmiHeader; } BITMAPINFO;
#define BI_RGB 0
#define DIB_RGB_COLORS 0
#endif
#define ARRAYSIZE(a) (sizeof(a)/sizeof(a[0]))

// Listbox
#define LBN_SELCHANGE 1
#define LB_GETSEL 0x0187
#define LB_RESETCONTENT 0x0184
#define LB_SETSEL 0x0185
#define IDR_IMAGE1 0

// UpDown
typedef struct { NMHDR hdr; int iPos; int iDelta; } NMUPDOWN;
#define UDN_DELTAPOS (-722)

// COM GUIDs (unused stubs)
#ifndef GUID_DEFINED
#define GUID_DEFINED
typedef struct { DWORD Data1; WORD Data2; WORD Data3; BYTE Data4[8]; } GUID;
#endif
#define GUID_ContainerFormatBmp GUID{}
#define GUID_ContainerFormatPng GUID{}
#define GUID_ContainerFormatJpeg GUID{}
#define GUID_ContainerFormatTiff GUID{}
#define GUID_WICPixelFormat24bppBGR GUID{}
#define GUID_WICPixelFormat32bppBGR GUID{}

// WIC imaging factory is typedef'd in GraphicsAPI.cpp where needed
#define WICDecodeMetadataCacheOnDemand 0
#define WICBitmapDitherTypeNone 0
#define WICBitmapPaletteTypeCustom 0
#define WICBitmapEncoderNoCache 0
#define WICBitmapInterpolationModeFant 0

// VT_R4 for variant types
#define VT_R4 4

// File I/O
#define GENERIC_READ  0x80000000L
#define GENERIC_WRITE 0x40000000L
#define ERROR_PATH_NOT_FOUND 3

// oapiDebugString is defined in OrbiterAPI.cpp

// Additional dialog/UI stubs
inline BOOL EndDialog(HWND, INT_PTR) { return FALSE; }
// LoadBitmap now in OrbiterPlatform.h
#define _access access
#define IDCANCEL 2
#define IDOK 1
#define IDHELP 9
#define IDYES 6
#define IDNO 7
#define BST_CHECKED 1
#define BST_UNCHECKED 0
#ifndef BM_GETCHECK
#define BM_GETCHECK 0x00F0
#endif
#ifndef BM_SETCHECK
#define BM_SETCHECK 0x00F1
#endif
#define BM_SETSTATE 0x00F3
#define WM_GETTEXT 0x000D
#define WM_SETTEXT 0x000C
#define EM_SETREADONLY 0x00CF
#define ES_READONLY 0x0800
inline BOOL CheckDlgButton(HWND, int, UINT) { return FALSE; }
inline UINT IsDlgButtonChecked(HWND, int) { return 0; }
inline BOOL SetDlgItemText(HWND, int, const char*) { return FALSE; }
inline UINT GetDlgItemText(HWND, int, char*, int) { return 0; }
inline int GetDlgItemInt(HWND, int, BOOL*, BOOL) { return 0; }
inline BOOL SetDlgItemInt(HWND, int, UINT, BOOL) { return FALSE; }

// Version info structure
typedef struct { DWORD dwSignature; DWORD dwFileVersionMS; DWORD dwFileVersionLS; } VS_FIXEDFILEINFO;

// Window class (for GraphicsAPI.cpp)
typedef struct { DWORD style; WNDPROC lpfnWndProc; int cbClsExtra; int cbWndExtra; HINSTANCE hInstance; HICON hIcon; HCURSOR hCursor; HBRUSH hbrBackground; const char* lpszMenuName; const char* lpszClassName; } WNDCLASS;

// Tab control (if not already defined by OrbiterPlatform.h)
#ifndef TCM_INSERTITEM
#define TCM_INSERTITEM 0x1307
#endif
#ifndef TCIF_TEXT
#define TCIF_TEXT 1
typedef struct { UINT mask; char* pszText; int cchTextMax; int iImage; LPARAM lParam; } TCITEM;
typedef TCITEM TC_ITEM;
inline int TabCtrl_GetCurSel(HWND) { return 0; }
inline int TabCtrl_InsertItem(HWND, int, const TCITEM*) { return 0; }
#define TCN_SELCHANGE (-551)
#endif

// Common controls init
#ifndef ICC_TREEVIEW_CLASSES
typedef struct { DWORD dwSize; DWORD dwICC; } INITCOMMONCONTROLSEX;
#define ICC_TREEVIEW_CLASSES 0x0002
inline BOOL InitCommonControlsEx(const INITCOMMONCONTROLSEX*) { return FALSE; }
#endif

#endif // !_WIN32

#endif // __RESOURCE_STUB_H
