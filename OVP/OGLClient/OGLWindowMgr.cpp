// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef _WIN32

#include "OGLWindowMgr.h"
#include "imgui.h"
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <memory>

namespace ogl {

OGLWindowMgr &OGLWindowMgr::Instance()
{
	static OGLWindowMgr s;
	return s;
}

OGLWindowMgr::Node *OGLWindowMgr::Find(std::uint64_t id)
{
	for (auto &n : m_nodes) if (n.id == id) return &n;
	return nullptr;
}

// ---- legacy HWND API ----
// On POSIX we have no HWND-hosted child dialogs; the addon is expected
// to register through the *ImGui overloads. We still accept the call
// so source-compatible D3D9 plugins don't crash — they just get no
// visible UI.
HNODE OGLWindowMgr::RegisterApplication(gcGUIApp *pApp, const char *label,
	HWND, DWORD docked, DWORD color)
{
	Node n{ m_nextId++, pApp, label ? label : "", {}, color,
	        docked, true, true, 0 };
	m_nodes.push_back(std::move(n));
	return reinterpret_cast<HNODE>((uintptr_t)m_nodes.back().id);
}

HNODE OGLWindowMgr::RegisterSubsection(HNODE hNode, const char *label,
	HWND, DWORD color)
{
	Node n{ m_nextId++, nullptr, label ? label : "", {}, color,
	        0, true, true, (std::uint64_t)(uintptr_t)hNode };
	m_nodes.push_back(std::move(n));
	return reinterpret_cast<HNODE>((uintptr_t)m_nodes.back().id);
}

void OGLWindowMgr::UpdateStatus(HNODE hNode, const char *label, HWND, DWORD color)
{
	Node *n = Find((std::uint64_t)(uintptr_t)hNode);
	if (!n) return;
	if (label) n->label = label;
	n->color = color;
}

// ---- cross-platform ImGui API ----
HNODE OGLWindowMgr::RegisterApplicationImGui(gcGUIApp *pApp, const char *label,
	gcGUIRender render, DWORD docked, DWORD color)
{
	Node n{ m_nextId++, pApp, label ? label : "", std::move(render),
	        color, docked, true, true, 0 };
	m_nodes.push_back(std::move(n));
	return reinterpret_cast<HNODE>((uintptr_t)m_nodes.back().id);
}

HNODE OGLWindowMgr::RegisterSubsectionImGui(HNODE hNode, const char *label,
	gcGUIRender render, DWORD color)
{
	Node n{ m_nextId++, nullptr, label ? label : "", std::move(render),
	        color, 0, true, true, (std::uint64_t)(uintptr_t)hNode };
	m_nodes.push_back(std::move(n));
	return reinterpret_cast<HNODE>((uintptr_t)m_nodes.back().id);
}

// ---- state ----
bool OGLWindowMgr::IsOpen(HNODE hNode)
{
	Node *n = Find((std::uint64_t)(uintptr_t)hNode);
	return n && n->open;
}

void OGLWindowMgr::OpenNode(HNODE hNode, bool bOpen)
{
	Node *n = Find((std::uint64_t)(uintptr_t)hNode);
	if (n) n->open = bOpen;
}

void OGLWindowMgr::DisplayWindow(HNODE hNode, bool bShow)
{
	Node *n = Find((std::uint64_t)(uintptr_t)hNode);
	if (n) n->shown = bShow;
}

HFONT OGLWindowMgr::GetFont(int)            { return nullptr; }
HNODE OGLWindowMgr::GetNode(HWND)           { return nullptr; }
HWND  OGLWindowMgr::GetDialog(HNODE)        { return nullptr; }
void  OGLWindowMgr::UpdateSize(HWND)        {}

bool OGLWindowMgr::UnRegister(HNODE hNode)
{
	auto id = (std::uint64_t)(uintptr_t)hNode;
	auto it = std::find_if(m_nodes.begin(), m_nodes.end(),
		[id](const Node &n) { return n.id == id; });
	if (it == m_nodes.end()) return false;

	// Notify the app and reparent any orphaned subsections to root.
	if (it->app) it->app->clbkMessage(gcGUI::MSG_CLOSE_APP, hNode, 0);
	for (auto &n : m_nodes) if (n.parent == id) n.parent = 0;
	m_nodes.erase(it);
	return true;
}

// ---- per-frame render ----
void OGLWindowMgr::RenderApplicationWindow(Node &root)
{
	if (!root.shown) return;

	// Default position: dock-left or dock-right, otherwise floating
	// near top-right. Applied on first appearance only — the user
	// can drag/resize freely from then on.
	ImGuiViewport *vp = ImGui::GetMainViewport();
	ImVec2 wsz(380.0f, 480.0f);
	ImVec2 wpos(vp->WorkPos.x + vp->WorkSize.x - wsz.x - 12.0f,
	            vp->WorkPos.y + 12.0f);
	if (root.docked == gcGUI::DS_LEFT)
		wpos.x = vp->WorkPos.x + 12.0f;
	else if (root.docked == gcGUI::DS_FLOAT)
		wpos.x = vp->WorkPos.x + vp->WorkSize.x * 0.5f - wsz.x * 0.5f;

	ImGui::SetNextWindowPos(wpos, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(wsz, ImGuiCond_FirstUseEver);

	bool open = root.open;
	std::string title = root.label;
	title += "##gcgui_";
	title += std::to_string(root.id);
	if (ImGui::Begin(title.c_str(), &open, ImGuiWindowFlags_NoSavedSettings)) {
		// Application body
		if (root.render) root.render();

		// Sub-sections in CollapsingHeaders
		for (auto &sub : m_nodes) {
			if (sub.parent != root.id) continue;
			if (!sub.shown) continue;
			ImGui::Spacing();
			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_None;
			if (sub.open) flags |= ImGuiTreeNodeFlags_DefaultOpen;
			if (ImGui::CollapsingHeader(sub.label.c_str(), flags)) {
				sub.open = true;
				ImGui::Indent();
				if (sub.render) sub.render();
				ImGui::Unindent();
			} else {
				sub.open = false;
			}
		}
	}
	ImGui::End();

	// Window-close X clicked
	if (!open && root.open) {
		root.open = false;
		if (root.app) root.app->clbkMessage(gcGUI::MSG_CLOSE_NODE,
			reinterpret_cast<HNODE>((uintptr_t)root.id), 0);
	}
}

void OGLWindowMgr::RenderAll()
{
	// Iterate by index because subsections walk the same vector. We
	// only render root nodes here; subsections are drawn inside their
	// parent's CollapsingHeader.
	for (size_t i = 0; i < m_nodes.size(); ++i) {
		if (m_nodes[i].parent == 0)
			RenderApplicationWindow(m_nodes[i]);
	}
}

} // namespace ogl

// ---- exported entry point ----
extern "C" gcGUIBase *gcGetGUICore()
{
	return &ogl::OGLWindowMgr::Instance();
}

// ---- smoke-test demo app ----
// Activated when the ORBITER_GCGUI_TEST environment variable is set.
// Registers a tiny gcGUIApp via the cross-platform Initialize() →
// dlsym(RTLD_DEFAULT, "gcGetGUICore") path so a dev can verify the
// addon-side wiring without needing a real plugin in the tree.

namespace ogl {

class GcGuiSmokeApp : public gcGUIApp
{
public:
	void Render() {
		++m_frame;
		ImGui::Text("OGLWindowMgr smoke test");
		ImGui::Separator();
		ImGui::Text("Frame counter: %llu",
			(unsigned long long)m_frame);
		ImGui::Checkbox("Enable click counter", &m_enabled);
		if (m_enabled) {
			if (ImGui::Button("Click me"))
				++m_clicks;
			ImGui::SameLine();
			ImGui::Text("Clicks: %d", m_clicks);
		}
	}

	bool clbkMessage(DWORD uMsg, HNODE hNode, int data) override {
		if (uMsg == gcGUI::MSG_CLOSE_NODE)
			fprintf(stderr, "[gcGUI smoke] node close\n");
		return false;
	}

private:
	std::uint64_t m_frame = 0;
	bool m_enabled = false;
	int m_clicks = 0;
};

void MaybeStartGcGuiSmokeApp()
{
	const char *flag = std::getenv("ORBITER_GCGUI_TEST");
	if (!flag || !*flag || flag[0] == '0') return;

	static std::unique_ptr<GcGuiSmokeApp> s_app;
	if (s_app) return; // idempotent

	s_app = std::make_unique<GcGuiSmokeApp>();
	if (!s_app->Initialize()) {
		fprintf(stderr, "[gcGUI smoke] Initialize() failed — no gcGetGUICore symbol\n");
		s_app.reset();
		return;
	}

	auto *app = s_app.get();
	HNODE h = s_app->RegisterApplicationImGui("gcGUI smoke test",
		[app]() { app->Render(); }, gcGUI::DS_RIGHT, 0);
	if (!h) {
		fprintf(stderr, "[gcGUI smoke] RegisterApplicationImGui returned null\n");
		s_app.reset();
		return;
	}
	fprintf(stderr, "[gcGUI smoke] registered demo app — toggle with the window's X\n");
}

} // namespace ogl

#endif // !_WIN32
