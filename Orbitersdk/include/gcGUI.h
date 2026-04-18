// ===================================================================
// Copyright (C) 2021 Jarmo Nikkanen
// Copyright (c) Orbiter macOS port contributors
// licensed under LGPL v2
//
// gcGUI — addon-facing API for registering side-bar applications and
// sub-section panels with the Orbiter graphics client at run time.
//
// Cross-platform layout:
//   * Win32      : the D3D9Client side bar (OVP/D3D9Client/WindowMgr)
//                  hosts HWND-based child dialogs. The legacy
//                  RegisterApplication / RegisterSubsection overloads
//                  taking HWND are the canonical API.
//   * macOS / Linux : the OGLClient side bar (OVP/OGLClient/OGLWindowMgr)
//                  hosts ImGui-based panels supplied as
//                  std::function<void()> render callbacks. The
//                  HWND-taking overloads are accepted for source
//                  compatibility but treated as no-ops (return 0).
//
// Existing D3D9 plugins keep compiling unchanged on Windows.
// New cross-platform plugins should call the *ImGui overloads which
// are available on every platform.
// ===================================================================

#ifndef __GC_GUI
#define __GC_GUI

#include "OrbiterAPI.h"
#include "DrawAPI.h"
#include <assert.h>
#include <functional>
#ifndef _WIN32
#  include <dlfcn.h>
#endif

using namespace std;
using namespace oapi;

namespace gcGUI
{
	// -----------------------------
	// Dialog status identifiers
	//
	static const int INACTIVE = 0;
	static const int DS_FLOAT = 1;
	static const int DS_LEFT  = 2;
	static const int DS_RIGHT = 3;

	// -----------------------------
	// Bitmap Identifiers
	//
	static const int BM_TITLE    = 0;
	static const int BM_SUBTITLE = 1;
	static const int BM_ICONS    = 2;

	// -----------------------------
	// Messages passed to gcGUIApp::clbkMessage
	//
	static const int MSG_OPEN_NODE  = 1;
	static const int MSG_CLOSE_NODE = 2;
	static const int MSG_CLOSE_APP  = 3;
}

typedef void *HNODE;

// -------------------------------------------------------------------
// Render callback type for the cross-platform path. The callback is
// invoked once per frame from inside the host's BeginChild / EndChild
// scope so the addon can issue ordinary ImGui draw calls.
typedef std::function<void()> gcGUIRender;

class gcGUIApp;

class gcGUIBase
{
	friend class gcGUIApp;

public:
	virtual ~gcGUIBase() {}

	// -----------------------------------------------------------------
	// Legacy HWND-based API (Win32 D3D9Client). On non-Windows
	// implementations these return 0 / do nothing — addons that want
	// portable behaviour should use the *ImGui overloads below.
	virtual HNODE RegisterApplication(gcGUIApp *pApp, const char *label,
		HWND hDlg, DWORD docked, DWORD color) = 0;
	virtual HNODE RegisterSubsection(HNODE hNode, const char *label,
		HWND hDlg, DWORD color) = 0;
	virtual void  UpdateStatus(HNODE hNode, const char *label,
		HWND hDlg, DWORD color) = 0;

	// -----------------------------------------------------------------
	// New ImGui-based API. The render callback is invoked every frame
	// while the node is open; the host wraps it in a child window and
	// handles the title bar, dock state and close button.
	virtual HNODE RegisterApplicationImGui(gcGUIApp *pApp, const char *label,
		gcGUIRender render, DWORD docked, DWORD color) = 0;
	virtual HNODE RegisterSubsectionImGui(HNODE hNode, const char *label,
		gcGUIRender render, DWORD color) = 0;

	// -----------------------------------------------------------------
	// State queries / mutators (cross-platform)
	virtual bool  IsOpen(HNODE hNode) = 0;
	virtual void  OpenNode(HNODE hNode, bool bOpen = true) = 0;
	virtual void  DisplayWindow(HNODE hNode, bool bShow = true) = 0;
	virtual HFONT GetFont(int id) = 0;
	virtual HNODE GetNode(HWND hDlg) = 0;
	virtual HWND  GetDialog(HNODE hNode) = 0;
	virtual void  UpdateSize(HWND hDlg) = 0;
	virtual bool  UnRegister(HNODE hNode) = 0;
};


// ===================================================================
/**
 * \class gcGUIApp
 * \brief gcGUI access and management functions
 */
// ===================================================================

class gcGUIApp
{
public:
	gcGUIApp() : pApp(nullptr) {}

	virtual ~gcGUIApp()
	{
		// Can do nothing here, too late
	}

	// -----------------------------------------------------------------
	// Subclass hooks
	virtual void clbkShutdown() {}

	virtual bool clbkMessage(DWORD uMsg, HNODE hNode, int data)
	{
		return false;
	}

	// -----------------------------------------------------------------
	// Bind to the host's gcGUIBase implementation. Looks up the
	// gcGetGUICore symbol exported by the active graphics client:
	//   * Win32 → GetProcAddress(D3D9Client.dll)
	//   * macOS / Linux → dlsym(RTLD_DEFAULT) (OGLClient is statically
	//     linked into the Orbiter binary, so the symbol is global).
	inline bool Initialize()
	{
		typedef gcGUIBase * (*__gcGetGUICore)();
		__gcGetGUICore pGetGUICore = nullptr;
#ifdef _WIN32
		HMODULE hModule = GetModuleHandle("D3D9Client.dll");
		if (hModule)
			pGetGUICore = (__gcGetGUICore)GetProcAddress(hModule, "gcGetGUICore");
#else
		// On POSIX the OGLClient sits in the main executable, so the
		// global symbol table contains gcGetGUICore.
		pGetGUICore = (__gcGetGUICore)dlsym(RTLD_DEFAULT, "gcGetGUICore");
#endif
		if (pGetGUICore) return ((pApp = pGetGUICore()) != nullptr);
		return false;
	}

	// Convenience forwarders
	HNODE RegisterApplication(const char *label, HWND hDlg, DWORD docked,
		DWORD color = 0)
	{
		assert(pApp);
		return pApp->RegisterApplication(this, label, hDlg, docked, color);
	}

	HNODE RegisterSubsection(HNODE hNode, const char *label, HWND hDlg,
		DWORD color = 0)
	{
		assert(pApp);
		return pApp->RegisterSubsection(hNode, label, hDlg, color);
	}

	HNODE RegisterApplicationImGui(const char *label, gcGUIRender render,
		DWORD docked, DWORD color = 0)
	{
		assert(pApp);
		return pApp->RegisterApplicationImGui(this, label, render, docked, color);
	}

	HNODE RegisterSubsectionImGui(HNODE hNode, const char *label,
		gcGUIRender render, DWORD color = 0)
	{
		assert(pApp);
		return pApp->RegisterSubsectionImGui(hNode, label, render, color);
	}

	void UpdateStatus(HNODE hNode, const char *label, HWND hDlg, DWORD color = 0)
	{
		assert(pApp);
		pApp->UpdateStatus(hNode, label, hDlg, color);
	}

	bool  IsOpen(HNODE hNode)                  { assert(pApp); return pApp->IsOpen(hNode); }
	void  OpenNode(HNODE hNode, bool b = true) { assert(pApp); pApp->OpenNode(hNode, b); }
	void  DisplayWindow(HNODE hNode, bool b = true) { assert(pApp); pApp->DisplayWindow(hNode, b); }
	HFONT GetFont(int id)                      { assert(pApp); return pApp->GetFont(id); }
	HNODE GetNode(HWND hDlg)                   { assert(pApp); return pApp->GetNode(hDlg); }
	HWND  GetDialog(HNODE hNode)               { assert(pApp); return pApp->GetDialog(hNode); }
	void  UpdateSize(HWND hDlg)                { assert(pApp); pApp->UpdateSize(hDlg); }
	bool  UnRegister(HNODE hNode)              { assert(pApp); return pApp->UnRegister(hNode); }

private:
	gcGUIBase *pApp;
};

#endif // __GC_GUI
