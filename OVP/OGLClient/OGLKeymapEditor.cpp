// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef _WIN32

#include "OGLKeymapEditor.h"
#include "Orbiter.h"
#include "imgui.h"
#include <cstring>
#include <cstdio>
#include <vector>
#include <algorithm>
#include <cctype>

extern Orbiter *g_pOrbiter;

namespace ogl {

namespace {

// US-ANSI keyboard rows. Each entry is { OAPI_KEY_*, "label", width
// in cell units }. Width 0 = standard 1.0 cell. We hand-craft only
// the rows the user is likely to remap (function keys + main block
// + numpad). Special navigation keys (HOME, END, ARROWS, ...) get
// their own compact block on the right.

struct KeyCell {
	WORD        key;
	const char *label;
	float       width; // 0 = standard 1.0
};

const KeyCell kRowFn[] = {
	{OAPI_KEY_ESCAPE, "Esc", 1},
	{0, "", 0.5f},
	{OAPI_KEY_F1, "F1", 1}, {OAPI_KEY_F2, "F2", 1},
	{OAPI_KEY_F3, "F3", 1}, {OAPI_KEY_F4, "F4", 1},
	{0, "", 0.25f},
	{OAPI_KEY_F5, "F5", 1}, {OAPI_KEY_F6, "F6", 1},
	{OAPI_KEY_F7, "F7", 1}, {OAPI_KEY_F8, "F8", 1},
	{0, "", 0.25f},
	{OAPI_KEY_F9, "F9", 1}, {OAPI_KEY_F10, "F10", 1},
	{OAPI_KEY_F11, "F11", 1}, {OAPI_KEY_F12, "F12", 1},
};

const KeyCell kRowNum[] = {
	{OAPI_KEY_GRAVE, "`", 1}, {OAPI_KEY_1, "1", 1}, {OAPI_KEY_2, "2", 1},
	{OAPI_KEY_3, "3", 1}, {OAPI_KEY_4, "4", 1}, {OAPI_KEY_5, "5", 1},
	{OAPI_KEY_6, "6", 1}, {OAPI_KEY_7, "7", 1}, {OAPI_KEY_8, "8", 1},
	{OAPI_KEY_9, "9", 1}, {OAPI_KEY_0, "0", 1},
	{OAPI_KEY_MINUS, "-", 1}, {OAPI_KEY_EQUALS, "=", 1},
	{OAPI_KEY_BACK, "Backspace", 1.5f},
};

const KeyCell kRowQ[] = {
	{OAPI_KEY_TAB, "Tab", 1.25f},
	{OAPI_KEY_Q, "Q", 1}, {OAPI_KEY_W, "W", 1}, {OAPI_KEY_E, "E", 1},
	{OAPI_KEY_R, "R", 1}, {OAPI_KEY_T, "T", 1}, {OAPI_KEY_Y, "Y", 1},
	{OAPI_KEY_U, "U", 1}, {OAPI_KEY_I, "I", 1}, {OAPI_KEY_O, "O", 1},
	{OAPI_KEY_P, "P", 1},
	{OAPI_KEY_LBRACKET, "[", 1}, {OAPI_KEY_RBRACKET, "]", 1},
	{OAPI_KEY_BACKSLASH, "\\", 1},
};

const KeyCell kRowA[] = {
	{OAPI_KEY_CAPITAL, "Caps", 1.5f},
	{OAPI_KEY_A, "A", 1}, {OAPI_KEY_S, "S", 1}, {OAPI_KEY_D, "D", 1},
	{OAPI_KEY_F, "F", 1}, {OAPI_KEY_G, "G", 1}, {OAPI_KEY_H, "H", 1},
	{OAPI_KEY_J, "J", 1}, {OAPI_KEY_K, "K", 1}, {OAPI_KEY_L, "L", 1},
	{OAPI_KEY_SEMICOLON, ";", 1}, {OAPI_KEY_APOSTROPHE, "'", 1},
	{OAPI_KEY_RETURN, "Enter", 1.75f},
};

const KeyCell kRowZ[] = {
	{OAPI_KEY_LSHIFT, "Shift", 1.75f},
	{OAPI_KEY_Z, "Z", 1}, {OAPI_KEY_X, "X", 1}, {OAPI_KEY_C, "C", 1},
	{OAPI_KEY_V, "V", 1}, {OAPI_KEY_B, "B", 1}, {OAPI_KEY_N, "N", 1},
	{OAPI_KEY_M, "M", 1}, {OAPI_KEY_COMMA, ",", 1}, {OAPI_KEY_PERIOD, ".", 1},
	{OAPI_KEY_SLASH, "/", 1},
	{OAPI_KEY_RSHIFT, "Shift", 1.75f},
};

const KeyCell kRowSpace[] = {
	{OAPI_KEY_LCONTROL, "Ctrl", 1.25f},
	{OAPI_KEY_LALT,     "Alt",  1.25f},
	{OAPI_KEY_SPACE,    "Space", 6.0f},
	{OAPI_KEY_LALT,     "Alt",  1.25f},        // RALT not exposed; reuse LALT
	{OAPI_KEY_RCONTROL, "Ctrl", 1.25f},
};

bool ContainsKey(WORD binding, WORD key)
{
	return (binding & 0xFF) == key;
}

bool AnyActionBoundTo(const Keymap *km, WORD key)
{
	for (int i = 0; i < Keymap::LogicalKeyCount(); ++i) {
		if (ContainsKey(km->GetBinding(i), key)) return true;
	}
	return false;
}

} // namespace

OGLKeymapEditor::OGLKeymapEditor()
	: ImGuiDialog("Keymap Editor", {880, 540})
{
}

void OGLKeymapEditor::OnDraw()
{
	if (!m_km) m_km = &g_pOrbiter->keymap;
	if (!m_km) {
		ImGui::TextDisabled("Keymap unavailable.");
		return;
	}

	if (ImGui::BeginTable("KmRoot", 2, ImGuiTableFlags_Resizable)) {
		ImGui::TableSetupColumn("Actions",  ImGuiTableColumnFlags_WidthStretch, 0.4f);
		ImGui::TableSetupColumn("Keyboard", ImGuiTableColumnFlags_WidthStretch, 0.6f);

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		RenderActionList();

		ImGui::TableNextColumn();
		RenderKeyboard();

		ImGui::EndTable();
	}

	ImGui::Separator();
	RenderFooter();
}

void OGLKeymapEditor::RenderActionList()
{
	char buf[128] = {};
	std::strncpy(buf, m_filter.c_str(), sizeof(buf) - 1);
	if (ImGui::InputText("Filter", buf, sizeof(buf)))
		m_filter = buf;

	if (ImGui::BeginTable("KmActions", 2,
		ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
		ImGuiTableFlags_ScrollY,
		ImVec2(0, ImGui::GetContentRegionAvail().y - 30.0f)))
	{
		ImGui::TableSetupColumn("Action");
		ImGui::TableSetupColumn("Binding");
		ImGui::TableHeadersRow();

		std::string lf = m_filter;
		std::transform(lf.begin(), lf.end(), lf.begin(),
			[](unsigned char c){ return std::tolower(c); });

		for (int i = 0; i < Keymap::LogicalKeyCount(); ++i) {
			std::string name = Keymap::LogicalKeyName(i);
			if (!lf.empty()) {
				std::string lname = name;
				std::transform(lname.begin(), lname.end(), lname.begin(),
					[](unsigned char c){ return std::tolower(c); });
				if (lname.find(lf) == std::string::npos) continue;
			}
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			bool sel = (m_capturing == i);
			if (ImGui::Selectable(name.c_str(), sel,
				ImGuiSelectableFlags_SpanAllColumns)) {
				m_capturing = sel ? -1 : i;
				m_status.clear();
			}
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(Pretty(m_km->GetBinding(i)).c_str());
		}
		ImGui::EndTable();
	}

	if (m_capturing >= 0) {
		ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f),
			"Capturing — press the new key combination "
			"(modifiers + key). Esc cancels.");
		WORD binding = 0;
		if (CaptureFromImGui(binding)) {
			m_km->SetBinding(m_capturing, binding);
			m_capturing = -1;
		}
	}
}

void OGLKeymapEditor::RenderKeyboard()
{
	auto drawRow = [&](const KeyCell *row, size_t n) {
		float cellH = 28.0f;
		float unit  = 38.0f;
		bool first = true;
		for (size_t i = 0; i < n; ++i) {
			const KeyCell &c = row[i];
			if (!first) ImGui::SameLine(0.0f, 2.0f);
			first = false;
			float w = (c.width > 0 ? c.width : 1.0f) * unit;
			if (c.key == 0) {
				ImGui::Dummy(ImVec2(w, cellH));
				continue;
			}
			ImVec4 col = ImGui::GetStyleColorVec4(ImGuiCol_Button);
			if (m_capturing >= 0 &&
				ContainsKey(m_km->GetBinding(m_capturing), c.key))
				col = ImVec4(1.0f, 0.85f, 0.4f, 1.0f);
			else if (AnyActionBoundTo(m_km, c.key))
				col = ImVec4(0.25f, 0.55f, 0.85f, 1.0f);
			ImGui::PushStyleColor(ImGuiCol_Button, col);
			ImGui::Button(c.label, ImVec2(w, cellH));
			if (ImGui::IsItemHovered()) {
				char buf[64];
				std::snprintf(buf, sizeof(buf), "Key 0x%02X", c.key);
				ImGui::SetTooltip("%s", buf);
			}
			ImGui::PopStyleColor();
		}
	};

	drawRow(kRowFn,    IM_ARRAYSIZE(kRowFn));
	drawRow(kRowNum,   IM_ARRAYSIZE(kRowNum));
	drawRow(kRowQ,     IM_ARRAYSIZE(kRowQ));
	drawRow(kRowA,     IM_ARRAYSIZE(kRowA));
	drawRow(kRowZ,     IM_ARRAYSIZE(kRowZ));
	drawRow(kRowSpace, IM_ARRAYSIZE(kRowSpace));

	ImGui::Spacing();
	ImGui::TextDisabled("Blue = key has at least one binding. "
		"Yellow = bound to currently captured action.");
}

void OGLKeymapEditor::RenderFooter()
{
	if (m_capturing >= 0)
		ImGui::Text("Capture target: %s",
			Keymap::LogicalKeyName(m_capturing));
	else
		ImGui::Text("Click an action to remap it.");

	if (!m_status.empty()) {
		ImGui::SameLine();
		ImGui::TextDisabled("[%s]", m_status.c_str());
	}

	ImGui::SameLine(ImGui::GetContentRegionAvail().x - 280);
	if (ImGui::Button("Save")) {
		m_km->Write("keymap.cfg");
		m_status = "saved keymap.cfg";
	}
	ImGui::SameLine();
	if (ImGui::Button("Reload")) {
		if (!m_km->Read("keymap.cfg")) m_km->SetDefault();
		m_status = "reloaded";
	}
	ImGui::SameLine();
	if (ImGui::Button("Reset to defaults")) {
		m_km->SetDefault();
		m_status = "defaults restored (not yet saved)";
	}
}

bool OGLKeymapEditor::CaptureFromImGui(WORD &outBinding)
{
	// Cancel on Escape.
	if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
		m_capturing = -1;
		return false;
	}

	// OAPI_KEY_* codes match DirectInput DIK codes, which index the
	// physical-keyboard scan order (A=0x1E, S=0x1F, D=0x20, ..., W=0x11,
	// B=0x30, PERIOD=0x34). ImGuiKey_A..ImGuiKey_Z are alphabetical, so
	// earlier code that treated the two as contiguous mapped most keys to
	// unrelated OAPI codes — pressing `S` captured as `B`, `D` as `F`,
	// `W` as `PERIOD`, etc. Use an explicit alphabet→OAPI table instead.
	static const WORD kAlpha[26] = {
		OAPI_KEY_A, OAPI_KEY_B, OAPI_KEY_C, OAPI_KEY_D, OAPI_KEY_E,
		OAPI_KEY_F, OAPI_KEY_G, OAPI_KEY_H, OAPI_KEY_I, OAPI_KEY_J,
		OAPI_KEY_K, OAPI_KEY_L, OAPI_KEY_M, OAPI_KEY_N, OAPI_KEY_O,
		OAPI_KEY_P, OAPI_KEY_Q, OAPI_KEY_R, OAPI_KEY_S, OAPI_KEY_T,
		OAPI_KEY_U, OAPI_KEY_V, OAPI_KEY_W, OAPI_KEY_X, OAPI_KEY_Y,
		OAPI_KEY_Z,
	};
	// F-row: F1..F10 are contiguous in DIK (0x3B..0x44) but F11/F12 live
	// at 0x57/0x58 — a linear offset would send F11→NUMLOCK, F12→SCROLL.
	static const WORD kFRow[12] = {
		OAPI_KEY_F1, OAPI_KEY_F2, OAPI_KEY_F3,  OAPI_KEY_F4,
		OAPI_KEY_F5, OAPI_KEY_F6, OAPI_KEY_F7,  OAPI_KEY_F8,
		OAPI_KEY_F9, OAPI_KEY_F10, OAPI_KEY_F11, OAPI_KEY_F12,
	};

	WORD k = 0;
	for (ImGuiKey key = ImGuiKey_NamedKey_BEGIN;
		 key < ImGuiKey_NamedKey_END;
		 key = (ImGuiKey)(key + 1))
	{
		if (!ImGui::IsKeyPressed(key, false)) continue;
		if (key >= ImGuiKey_A && key <= ImGuiKey_Z)
			k = kAlpha[key - ImGuiKey_A];
		else if (key >= ImGuiKey_1 && key <= ImGuiKey_9)
			k = OAPI_KEY_1 + (key - ImGuiKey_1); // 1..9 are contiguous (0x02..0x0A)
		else if (key == ImGuiKey_0) k = OAPI_KEY_0; // 0 sits at 0x0B, past 9
		else if (key >= ImGuiKey_F1 && key <= ImGuiKey_F12)
			k = kFRow[key - ImGuiKey_F1];
		else if (key == ImGuiKey_Space)      k = OAPI_KEY_SPACE;
		else if (key == ImGuiKey_Enter)      k = OAPI_KEY_RETURN;
		else if (key == ImGuiKey_Tab)        k = OAPI_KEY_TAB;
		else if (key == ImGuiKey_LeftArrow)  k = OAPI_KEY_LEFT;
		else if (key == ImGuiKey_RightArrow) k = OAPI_KEY_RIGHT;
		else if (key == ImGuiKey_UpArrow)    k = OAPI_KEY_UP;
		else if (key == ImGuiKey_DownArrow)  k = OAPI_KEY_DOWN;
		else if (key == ImGuiKey_Home)       k = OAPI_KEY_HOME;
		else if (key == ImGuiKey_End)        k = OAPI_KEY_END;
		else if (key == ImGuiKey_PageUp)     k = OAPI_KEY_PRIOR;
		else if (key == ImGuiKey_PageDown)   k = OAPI_KEY_NEXT;
		else if (key == ImGuiKey_Insert)     k = OAPI_KEY_INSERT;
		else if (key == ImGuiKey_Delete)     k = OAPI_KEY_DELETE;
		else if (key == ImGuiKey_Backspace)  k = OAPI_KEY_BACK;
		else if (key == ImGuiKey_Comma)      k = OAPI_KEY_COMMA;
		else if (key == ImGuiKey_Period)     k = OAPI_KEY_PERIOD;
		else if (key == ImGuiKey_Slash)      k = OAPI_KEY_SLASH;
		else if (key == ImGuiKey_Minus)      k = OAPI_KEY_MINUS;
		else if (key == ImGuiKey_Equal)      k = OAPI_KEY_EQUALS;
		// OEM keys by physical scancode position — ImGui reports the
		// ISO/ANSI slot regardless of layout, so on Italian keyboards the
		// è / + / ò / à / ù / \ keys route through these ImGui enums.
		else if (key == ImGuiKey_LeftBracket)  k = OAPI_KEY_LBRACKET;
		else if (key == ImGuiKey_RightBracket) k = OAPI_KEY_RBRACKET;
		else if (key == ImGuiKey_Semicolon)    k = OAPI_KEY_SEMICOLON;
		else if (key == ImGuiKey_Apostrophe)   k = OAPI_KEY_APOSTROPHE;
		else if (key == ImGuiKey_Backslash)    k = OAPI_KEY_BACKSLASH;
		else if (key == ImGuiKey_GraveAccent)  k = OAPI_KEY_GRAVE;
		if (k) break;
	}
	if (!k) return false;

	WORD mod = 0;
	ImGuiIO &io = ImGui::GetIO();
	if (io.KeyCtrl)  mod |= KMOD_CTRL;
	if (io.KeyShift) mod |= KMOD_SHIFT;
	if (io.KeyAlt)   mod |= KMOD_ALT;

	outBinding = k | mod;
	return true;
}

std::string OGLKeymapEditor::Pretty(WORD binding) const
{
	if (!binding) return "(unbound)";
	char buf[64];
	m_km->FormatBinding(buf, binding);
	return buf;
}

void OpenKeymapEditor()
{
	if (!g_pOrbiter || !g_pOrbiter->DlgMgr()) return;
	g_pOrbiter->DlgMgr()->EnsureEntry<OGLKeymapEditor>();
}

} // namespace ogl

#endif // !_WIN32
