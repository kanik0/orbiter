// Copyright (c) Martin Schweiger
// Licensed under the MIT License
//
// OGLWindowMgr — ImGui-backed implementation of the gcGUIBase API
// declared in Orbitersdk/include/gcGUI.h.
//
// The Win32 D3D9Client variant (OVP/D3D9Client/WindowMgr) hosts plugin
// dialogs as HWND child windows inside a hand-painted dock side bar.
// On macOS / Linux we have no Win32 child-window machinery, so the
// OGL backend draws every registered application / subsection as a
// standalone ImGui window whose body comes from a render callback
// (gcGUIRender / std::function<void()>). Dock-left / dock-right /
// floating preferences are reduced to default-position hints and the
// ImGui window manager owns drag/resize/close interactively.
//
// gcGetGUICore() (extern "C") returns a pointer to the singleton.
// gcGUIApp::Initialize() resolves it via dlsym(RTLD_DEFAULT) on POSIX.

#ifndef __OGLWINDOWMGR_H
#define __OGLWINDOWMGR_H

#ifndef _WIN32

#include "gcGUI.h"
#include <string>
#include <vector>
#include <cstdint>

namespace ogl {

class OGLWindowMgr : public gcGUIBase
{
public:
	static OGLWindowMgr &Instance();

	// Render every registered application / subsection. Called once
	// per frame from inside the host's ImGui frame.
	void RenderAll();

	// ---- gcGUIBase: legacy HWND-based API (no-op on POSIX) ----
	HNODE RegisterApplication(gcGUIApp *pApp, const char *label,
		HWND hDlg, DWORD docked, DWORD color) override;
	HNODE RegisterSubsection(HNODE hNode, const char *label,
		HWND hDlg, DWORD color) override;
	void  UpdateStatus(HNODE hNode, const char *label,
		HWND hDlg, DWORD color) override;

	// ---- gcGUIBase: cross-platform ImGui API ----
	HNODE RegisterApplicationImGui(gcGUIApp *pApp, const char *label,
		gcGUIRender render, DWORD docked, DWORD color) override;
	HNODE RegisterSubsectionImGui(HNODE hNode, const char *label,
		gcGUIRender render, DWORD color) override;

	// ---- gcGUIBase: state queries / mutators ----
	bool  IsOpen(HNODE hNode) override;
	void  OpenNode(HNODE hNode, bool bOpen = true) override;
	void  DisplayWindow(HNODE hNode, bool bShow = true) override;
	HFONT GetFont(int id) override;
	HNODE GetNode(HWND hDlg) override;
	HWND  GetDialog(HNODE hNode) override;
	void  UpdateSize(HWND hDlg) override;
	bool  UnRegister(HNODE hNode) override;

private:
	OGLWindowMgr() = default;

	struct Node {
		std::uint64_t id;
		gcGUIApp     *app;
		std::string   label;
		gcGUIRender   render;     // empty for HWND-only registrations
		DWORD         color;
		DWORD         docked;
		bool          open;
		bool          shown;       // window visible toggle
		std::uint64_t parent;      // 0 for application root
	};

	Node *Find(std::uint64_t id);
	void  RenderApplicationWindow(Node &root);

	std::vector<Node> m_nodes;
	std::uint64_t     m_nextId = 1;
};

} // namespace ogl

extern "C" gcGUIBase *gcGetGUICore();

namespace ogl {
// If the ORBITER_GCGUI_TEST env var is set, instantiate and register a
// demo gcGUIApp that exercises the dlsym(RTLD_DEFAULT) → gcGetGUICore
// → RegisterApplicationImGui path. Idempotent. Called once from
// OGLClient::clbkImGuiInit.
void MaybeStartGcGuiSmokeApp();
}

#endif // !_WIN32
#endif // __OGLWINDOWMGR_H
