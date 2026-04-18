// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// OGLKeymapEditor — ImGui editor for the cross-platform Keymap.
//
// Lets the user inspect and remap any logical action (LKEY_*) used
// by Orbiter on macOS / Linux. On Windows the original DInput-based
// keyboard mapper continues to handle the Win32 path.
//
// Layout:
//   * Left pane: searchable two-column table of all LKEY_COUNT
//     logical actions (name + current binding); click a row to
//     enter capture mode for that action.
//   * Right pane: 2D US-ANSI keyboard layout where each cell is
//     coloured if at least one logical action binds to that key,
//     and tinted yellow while it is the active capture target.
//   * Footer: capture-status banner + Save / Reload / Reset buttons.
// Saves to keymap.cfg in the current working directory (the
// existing Keymap::Read/Write file).

#ifndef __OGL_KEYMAP_EDITOR_H
#define __OGL_KEYMAP_EDITOR_H

#ifndef _WIN32

#include "DlgMgr.h"
#include "Keymap.h"
#include <string>

namespace ogl {

class OGLKeymapEditor : public ImGuiDialog
{
public:
	OGLKeymapEditor();
	void OnDraw() override;

private:
	void RenderActionList();
	void RenderKeyboard();
	void RenderFooter();
	bool CaptureFromImGui(WORD &outBinding);
	std::string Pretty(WORD binding) const;

	Keymap *m_km = nullptr;     // resolved lazily on first frame
	int     m_capturing = -1;   // logical-action index awaiting input
	std::string m_filter;        // case-insensitive substring filter
	std::string m_status;        // last save / reload result
};

void OpenKeymapEditor();

} // namespace ogl

#endif // !_WIN32
#endif // __OGL_KEYMAP_EDITOR_H
