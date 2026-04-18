// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef _WIN32

#include "OGLLaunchpad.h"
#include "Config.h"
#include "imgui.h"
#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cstdio>

// stb_image — implementation lives in OGLTexture.cpp; here we only need the
// public interface to decode scenario thumbnails.
#include "stb_image.h"

// OpenGL — needed for direct texture upload of scenario thumbnails into the
// pre-game ImGui frame.
#if defined(__APPLE__)
#  define GL_SILENCE_DEPRECATION
#  include <OpenGL/gl3.h>
#else
#  include <GL/gl.h>
#endif

namespace ogl {

OGLLaunchpad::OGLLaunchpad()
{
	m_root.name = "Scenarios";
	m_root.isFolder = true;
	m_root.isOpen = true;
}

OGLLaunchpad::~OGLLaunchpad()
{
	ReleaseThumbnail();
}

void OGLLaunchpad::Bind(Config *cfg)
{
	m_cfg = cfg;
	InitFromConfig();
}

void OGLLaunchpad::InitFromConfig()
{
	if (!m_cfg) return;

	m_startPaused = m_cfg->CfgLogicPrm.bStartPaused;

	// Options tab
	m_opt.flightModelLevel = m_cfg->CfgLogicPrm.FlightModelLevel;
	m_opt.damageSetting    = m_cfg->CfgLogicPrm.DamageSetting;
	m_opt.bLimitedFuel     = m_cfg->CfgLogicPrm.bLimitedFuel;
	m_opt.bPadRefuel       = m_cfg->CfgLogicPrm.bPadRefuel;
	m_opt.mfdSize          = m_cfg->CfgLogicPrm.MFDSize;
	m_opt.mfdMapVersion    = m_cfg->CfgLogicPrm.MFDMapVersion;
	m_opt.bMfdTransparent  = m_cfg->CfgLogicPrm.bMfdTransparent;
	m_opt.bGlasspitCompact = m_cfg->CfgLogicPrm.bGlasspitCompact;
	m_opt.instrUpdDT       = m_cfg->CfgLogicPrm.InstrUpdDT;
	m_opt.panelScale       = m_cfg->CfgLogicPrm.PanelScale;
	m_opt.panelScrollSpeed = m_cfg->CfgLogicPrm.PanelScrollSpeed;

	// Video tab
	m_vid.bFullscreen = m_cfg->CfgDevPrm.bFullscreen;
	m_vid.bNoVsync    = m_cfg->CfgDevPrm.bNoVsync;
	m_vid.bStereo     = m_cfg->CfgDevPrm.bStereo;
	m_vid.bTryStencil = m_cfg->CfgDevPrm.bTryStencil;
	m_vid.winW        = (int)m_cfg->CfgDevPrm.WinW;
	m_vid.winH        = (int)m_cfg->CfgDevPrm.WinH;

	// Splitter positions persisted across runs
	if (m_cfg->CfgWindowPos.LaunchpadScnListWidth > 0)
		m_scn.splitterPos = std::min(0.85f, std::max(0.15f,
			(float)m_cfg->CfgWindowPos.LaunchpadScnListWidth / 1000.0f));
	if (m_cfg->CfgWindowPos.LaunchpadExtListWidth > 0)
		m_ext.splitterPos = std::min(0.85f, std::max(0.15f,
			(float)m_cfg->CfgWindowPos.LaunchpadExtListWidth / 1000.0f));
}

void OGLLaunchpad::SyncToConfig()
{
	if (!m_cfg) return;

	// Logic
	m_cfg->CfgLogicPrm.bStartPaused      = m_startPaused;
	m_cfg->CfgLogicPrm.FlightModelLevel  = m_opt.flightModelLevel;
	m_cfg->CfgLogicPrm.DamageSetting     = m_opt.damageSetting;
	m_cfg->CfgLogicPrm.bLimitedFuel      = m_opt.bLimitedFuel;
	m_cfg->CfgLogicPrm.bPadRefuel        = m_opt.bPadRefuel;
	m_cfg->CfgLogicPrm.MFDSize           = m_opt.mfdSize;
	m_cfg->CfgLogicPrm.MFDMapVersion     = m_opt.mfdMapVersion;
	m_cfg->CfgLogicPrm.bMfdTransparent   = m_opt.bMfdTransparent;
	m_cfg->CfgLogicPrm.bGlasspitCompact  = m_opt.bGlasspitCompact;
	m_cfg->CfgLogicPrm.InstrUpdDT        = m_opt.instrUpdDT;
	m_cfg->CfgLogicPrm.PanelScale        = m_opt.panelScale;
	m_cfg->CfgLogicPrm.PanelScrollSpeed  = m_opt.panelScrollSpeed;

	// Video
	m_cfg->CfgDevPrm.bFullscreen = m_vid.bFullscreen;
	m_cfg->CfgDevPrm.bNoVsync    = m_vid.bNoVsync;
	m_cfg->CfgDevPrm.bStereo     = m_vid.bStereo;
	m_cfg->CfgDevPrm.bTryStencil = m_vid.bTryStencil;
	m_cfg->CfgDevPrm.WinW        = (DWORD)m_vid.winW;
	m_cfg->CfgDevPrm.WinH        = (DWORD)m_vid.winH;

	// Persist splitter positions (encoded as 0..1000)
	m_cfg->CfgWindowPos.LaunchpadScnListWidth = (int)(m_scn.splitterPos * 1000.0f);
	m_cfg->CfgWindowPos.LaunchpadExtListWidth = (int)(m_ext.splitterPos * 1000.0f);
}

void OGLLaunchpad::ScanScenarios(const std::string &scenarioDir)
{
	m_root.children.clear();
	ScanDirectory(scenarioDir, "", m_root);
	m_scenariosLoaded = true;
	fprintf(stderr, "[OGLLaunchpad] Scanned scenarios from '%s'\n", scenarioDir.c_str());
}

void OGLLaunchpad::ScanDirectory(const std::string &dir, const std::string &relPath, ScenarioEntry &parent)
{
	DIR *d = opendir(dir.c_str());
	if (!d) return;

	std::vector<ScenarioEntry> folders, files;
	struct dirent *entry;
	while ((entry = readdir(d)) != nullptr) {
		if (entry->d_name[0] == '.') continue; // skip hidden

		std::string name = entry->d_name;
		std::string fullPath = dir + "/" + name;
		std::string rel = relPath.empty() ? name : relPath + "/" + name;

		struct stat st;
		if (stat(fullPath.c_str(), &st) != 0) continue;

		if (S_ISDIR(st.st_mode)) {
			ScenarioEntry fe;
			fe.name = name;
			fe.path = rel;
			fe.fullPath = fullPath;
			fe.isFolder = true;
			fe.isOpen = false;
			ScanDirectory(fullPath, rel, fe);
			if (!fe.children.empty())
				folders.push_back(std::move(fe));
		} else {
			size_t extPos = name.rfind(".scn");
			if (extPos == std::string::npos) continue;
			if (extPos + 4 != name.size()) continue;

			ScenarioEntry se;
			se.name = name.substr(0, extPos);
			se.path = rel.substr(0, rel.size() - 4);
			se.fullPath = fullPath;
			se.isFolder = false;
			se.isOpen = false;
			files.push_back(std::move(se));
		}
	}
	closedir(d);

	std::sort(folders.begin(), folders.end(), [](const ScenarioEntry &a, const ScenarioEntry &b) {
		return a.name < b.name;
	});
	std::sort(files.begin(), files.end(), [](const ScenarioEntry &a, const ScenarioEntry &b) {
		return a.name < b.name;
	});

	for (auto &f : folders) parent.children.push_back(std::move(f));
	for (auto &f : files) parent.children.push_back(std::move(f));
}

// Read a BEGIN_<block>/END_<block> block from the input stream and return its
// concatenated body. Mirrors Win32 ScanFileDesc() (TabScenario.cpp:316).
static std::string ScanBlock(std::istream &is, const std::string &blockName)
{
	std::string out;
	std::string begin = "BEGIN_" + blockName;
	std::string end   = "END_"   + blockName;

	std::string line;
	bool inBlock = false;
	while (std::getline(is, line)) {
		// strip trailing CR (Windows line endings) so prefix comparison works
		if (!line.empty() && line.back() == '\r') line.pop_back();

		if (!inBlock) {
			if (line.compare(0, begin.size(), begin) == 0) {
				inBlock = true;
			}
			continue;
		}
		if (line.compare(0, end.size(), end) == 0)
			break;
		if (!out.empty()) out += '\n';
		out += line;
	}
	return out;
}

std::string OGLLaunchpad::HtmlToPlainText(const std::string &html)
{
	std::string s;
	s.reserve(html.size());
	// 1. Convert paragraph and heading closers to blank lines (mirrors Html2Text).
	std::string in = html;
	auto replaceAll = [](std::string &str, const std::string &from, const std::string &to) {
		size_t pos = 0;
		while ((pos = str.find(from, pos)) != std::string::npos) {
			str.replace(pos, from.size(), to);
			pos += to.size();
		}
	};
	replaceAll(in, "</h1>", "\n\n");
	replaceAll(in, "</p>",  "\n\n");
	replaceAll(in, "<br />", "\n");
	replaceAll(in, "<br/>",  "\n");
	replaceAll(in, "<br>",   "\n");

	// 2. Strip remaining tags.
	bool inTag = false;
	for (char c : in) {
		if (c == '<') { inTag = true; continue; }
		if (c == '>') { inTag = false; continue; }
		if (!inTag) s += c;
	}

	// 3. Decode common HTML entities.
	replaceAll(s, "&gt;",  ">");
	replaceAll(s, "&lt;",  "<");
	replaceAll(s, "&ge;",  ">=");
	replaceAll(s, "&le;",  "<=");
	replaceAll(s, "&nbsp;", " ");
	replaceAll(s, "&amp;", "&"); // last so we don't double-decode

	// 4. Collapse runs of more than two consecutive newlines.
	std::string out;
	out.reserve(s.size());
	int nl = 0;
	for (char c : s) {
		if (c == '\n') {
			if (++nl <= 2) out += c;
		} else {
			nl = 0;
			out += c;
		}
	}
	return out;
}

void OGLLaunchpad::LoadDescription(const std::string &fullPath, bool isFolder)
{
	m_selectedDescription.clear();
	if (isFolder) {
		// Folders get their description from a sibling Description.txt with the
		// same DESC / HYPERDESC blocks used by the Windows Launchpad.
		std::ifstream f(fullPath + "/Description.txt");
		if (!f.is_open()) return;
		std::string desc = ScanBlock(f, "DESC");
		if (desc.empty()) {
			f.clear();
			f.seekg(0);
			std::string hyper = ScanBlock(f, "HYPERDESC");
			if (!hyper.empty()) desc = HtmlToPlainText(hyper);
		}
		m_selectedDescription = desc;
	} else {
		std::ifstream f(fullPath);
		if (!f.is_open()) return;
		m_selectedDescription = ScanBlock(f, "DESC");
	}
}

void OGLLaunchpad::ReleaseThumbnail()
{
	if (m_selectedThumb.texId) {
		GLuint t = (GLuint)m_selectedThumb.texId;
		glDeleteTextures(1, &t);
	}
	m_selectedThumb = ScenarioThumbnail{};
}

void OGLLaunchpad::LoadThumbnail(const std::string &scenarioFullPath)
{
	ReleaseThumbnail();

	// Strip .scn extension and try common image suffixes next to the file.
	std::string base = scenarioFullPath;
	if (base.size() > 4 && base.compare(base.size() - 4, 4, ".scn") == 0)
		base.resize(base.size() - 4);

	const char *exts[] = { ".jpg", ".jpeg", ".png", ".bmp" };
	std::string found;
	for (const char *e : exts) {
		std::string p = base + e;
		struct stat st;
		if (stat(p.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
			found = p;
			break;
		}
	}
	m_selectedThumb.tried = true;
	if (found.empty()) return;

	// Read whole file into memory — OGLClient builds stb_image with
	// STBI_NO_STDIO so we cannot use stbi_load() directly.
	std::ifstream ifs(found, std::ios::binary | std::ios::ate);
	if (!ifs) return;
	std::streamsize sz = ifs.tellg();
	if (sz <= 0) return;
	ifs.seekg(0, std::ios::beg);
	std::vector<unsigned char> bytes((size_t)sz);
	if (!ifs.read(reinterpret_cast<char*>(bytes.data()), sz)) return;

	int w, h, comp;
	stbi_set_flip_vertically_on_load(0);
	unsigned char *pix = stbi_load_from_memory(
		bytes.data(), (int)bytes.size(), &w, &h, &comp, 4);
	if (!pix) return;

	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pix);
	stbi_image_free(pix);

	m_selectedThumb.texId  = tex;
	m_selectedThumb.width  = w;
	m_selectedThumb.height = h;
}

// pendingLaunch is set true if the user double-clicked a scenario leaf.
static bool s_pendingLaunch = false;

void OGLLaunchpad::RenderTree(ScenarioEntry &entry)
{
	for (auto &child : entry.children) {
		if (child.isFolder) {
			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
			bool isSelected = (m_selectionIsFolder && m_selectedScenarioFull == child.fullPath);
			if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;
			bool nodeOpen = ImGui::TreeNodeEx(child.name.c_str(), flags);
			if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
				m_selectionIsFolder = true;
				m_selectedScenarioFull = child.fullPath;
				m_selectedScenario.clear();
				LoadDescription(child.fullPath, true);
				ReleaseThumbnail();
			}
			if (nodeOpen) {
				RenderTree(child);
				ImGui::TreePop();
			}
		} else {
			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
			bool isSelected = (!m_selectionIsFolder && m_selectedScenario == child.path);
			if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;

			ImGui::TreeNodeEx(child.name.c_str(), flags);
			if (ImGui::IsItemClicked()) {
				m_selectionIsFolder = false;
				m_selectedScenario = child.path;
				m_selectedScenarioFull = child.fullPath;
				LoadDescription(child.fullPath, false);
				LoadThumbnail(child.fullPath);
			}
			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
				m_selectionIsFolder = false;
				m_selectedScenario = child.path;
				m_selectedScenarioFull = child.fullPath;
				s_pendingLaunch = true;
			}
		}
	}
}

// Vertical splitter helper — draws an invisible button between two panes that
// the user can drag to repartition. Stores the new fraction (0.15 .. 0.85) in
// `frac`. Returns true while the user is actively dragging.
static bool VerticalSplitter(const char *id, float availW, float availH,
	float &frac, bool &dragging)
{
	const float thickness = 6.0f;
	float leftW = availW * frac;
	if (leftW < 120.0f) { leftW = 120.0f; frac = leftW / availW; }
	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_Separator));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_SeparatorHovered));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImGui::GetStyleColorVec4(ImGuiCol_SeparatorActive));
	ImGui::Button(id, ImVec2(thickness, availH));
	ImGui::PopStyleColor(3);
	if (ImGui::IsItemActive()) {
		dragging = true;
		float dx = ImGui::GetIO().MouseDelta.x;
		float newFrac = frac + dx / availW;
		if (newFrac < 0.15f) newFrac = 0.15f;
		if (newFrac > 0.85f) newFrac = 0.85f;
		frac = newFrac;
	} else if (ImGui::IsMouseReleased(0)) {
		dragging = false;
	}
	if (ImGui::IsItemHovered() || ImGui::IsItemActive())
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
	return dragging;
}

void OGLLaunchpad::RenderTabScenario(float availH)
{
	float availW = ImGui::GetContentRegionAvail().x;
	float leftW  = availW * m_scn.splitterPos;
	if (leftW < 150.0f) leftW = 150.0f;

	ImGui::BeginChild("ScenarioTree", ImVec2(leftW, availH), true);
	RenderTree(m_root);
	ImGui::EndChild();

	VerticalSplitter("##ScnSplit", availW, availH, m_scn.splitterPos, m_draggingScnSplitter);
	ImGui::SameLine();

	ImGui::BeginChild("ScenarioDesc", ImVec2(0, availH), true);
	if (m_selectionIsFolder && !m_selectedScenarioFull.empty()) {
		const char *name = strrchr(m_selectedScenarioFull.c_str(), '/');
		ImGui::TextWrapped("Folder: %s", name ? name + 1 : m_selectedScenarioFull.c_str());
		ImGui::Separator();
		if (!m_selectedDescription.empty())
			ImGui::TextWrapped("%s", m_selectedDescription.c_str());
		else
			ImGui::TextDisabled("(no Description.txt in this folder)");
	} else if (!m_selectedScenario.empty()) {
		ImGui::TextWrapped("Scenario: %s", m_selectedScenario.c_str());
		ImGui::Separator();
		if (m_selectedThumb.texId) {
			float pw = ImGui::GetContentRegionAvail().x;
			float aspect = (m_selectedThumb.height > 0)
				? (float)m_selectedThumb.width / (float)m_selectedThumb.height
				: 1.0f;
			float th = pw / aspect;
			float maxH = availH * 0.4f;
			if (th > maxH) { th = maxH; pw = th * aspect; }
			ImGui::Image((ImTextureID)(intptr_t)m_selectedThumb.texId, ImVec2(pw, th));
			ImGui::Separator();
		}
		if (!m_selectedDescription.empty())
			ImGui::TextWrapped("%s", m_selectedDescription.c_str());
		else
			ImGui::TextDisabled("(no description)");
	} else {
		ImGui::TextDisabled("Select a scenario to launch — double-click to start immediately.");
	}
	ImGui::EndChild();
}

void OGLLaunchpad::RenderTabOptions(float availH)
{
	ImGui::BeginChild("OptionsContent", ImVec2(0, availH), false);
	ImGui::TextDisabled("Options tab — implemented in M22.e");
	ImGui::EndChild();
}

void OGLLaunchpad::RenderTabModules(float availH)
{
	ImGui::BeginChild("ModulesContent", ImVec2(0, availH), false);
	ImGui::TextDisabled("Modules tab — implemented in M22.d");
	ImGui::EndChild();
}

void OGLLaunchpad::RenderTabVideo(float availH)
{
	ImGui::BeginChild("VideoContent", ImVec2(0, availH), false);
	ImGui::TextDisabled("Video tab — implemented in M22.c");
	ImGui::EndChild();
}

void OGLLaunchpad::RenderTabExtra(float availH)
{
	ImGui::BeginChild("ExtraContent", ImVec2(0, availH), false);
	ImGui::TextDisabled("Extra Parameters — implemented in M22.f/.g");
	ImGui::EndChild();
}

void OGLLaunchpad::RenderTabAbout(float availH)
{
	ImGui::BeginChild("AboutContent", ImVec2(0, availH), false);
	ImGui::Spacing();
	ImGui::Text("Orbiter Space Flight Simulator");
	ImGui::Text("macOS / Apple Silicon Port");
	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();
	ImGui::Text("Original: Martin Schweiger");
	ImGui::Text("OpenGL Client: OGLClient (OpenGL 4.1 + SDL2 + ImGui)");
	ImGui::Spacing();
	ImGui::TextWrapped("Orbiter is a realistic spaceflight simulation that lets you "
		"experience manned and unmanned space missions from the pilot's perspective.");
	ImGui::EndChild();
}

bool OGLLaunchpad::Render(bool &quit)
{
	quit = false;
	bool launch = false;

	ImGuiViewport *viewport = ImGui::GetMainViewport();
	ImVec2 center = viewport->GetCenter();

	float winW = viewport->Size.x * 0.7f;
	float winH = viewport->Size.y * 0.8f;
	if (winW < 700) winW = 700;
	if (winH < 500) winH = 500;

	ImGui::SetNextWindowPos(ImVec2(center.x - winW * 0.5f, center.y - winH * 0.5f), ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(winW, winH), ImGuiCond_Once);

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
	bool open = true;
	if (ImGui::Begin("Orbiter Space Flight Simulator", &open, flags)) {

		// Footer reserves vertical space (button row + separator + padding)
		const float footerH = ImGui::GetFrameHeightWithSpacing() + 16.0f;
		float bodyH = ImGui::GetContentRegionAvail().y - footerH;
		if (bodyH < 100.0f) bodyH = 100.0f;

		ImGui::BeginChild("LpadBody", ImVec2(0, bodyH), false);

		if (ImGui::BeginTabBar("LaunchpadTabs", ImGuiTabBarFlags_None)) {
			float tabH = ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing();
			if (tabH < 80.0f) tabH = 80.0f;

			struct { const char *label; int idx; } tabs[] = {
				{"Scenarios", 0}, {"Options", 1}, {"Modules", 2},
				{"Video",     3}, {"Extra",   4}, {"About",   5},
			};
			for (auto &t : tabs) {
				if (ImGui::BeginTabItem(t.label)) {
					m_currentTab = t.idx;
					switch (t.idx) {
						case 0: RenderTabScenario(tabH); break;
						case 1: RenderTabOptions (tabH); break;
						case 2: RenderTabModules (tabH); break;
						case 3: RenderTabVideo   (tabH); break;
						case 4: RenderTabExtra   (tabH); break;
						case 5: RenderTabAbout   (tabH); break;
					}
					ImGui::EndTabItem();
				}
			}
			ImGui::EndTabBar();
		}
		ImGui::EndChild();

		ImGui::Separator();

		// Footer: Start paused checkbox (left) + Launch / Exit (right)
		ImGui::Checkbox("Start paused", &m_startPaused);

		ImGui::SameLine();
		float bw = 130.0f;
		float gap = 8.0f;
		float total = bw * 2 + gap;
		ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - total);

		bool canLaunch = !m_selectedScenario.empty();
		if (!canLaunch) ImGui::BeginDisabled();
		if (ImGui::Button("Launch Orbiter", ImVec2(bw, 0)))
			launch = true;
		if (!canLaunch) ImGui::EndDisabled();

		ImGui::SameLine();
		if (ImGui::Button("Exit", ImVec2(bw, 0)))
			quit = true;
	}
	ImGui::End();

	if (!open) quit = true;

	if (s_pendingLaunch && !m_selectedScenario.empty()) {
		s_pendingLaunch = false;
		launch = true;
	}

	if (launch) SyncToConfig();

	return launch;
}

} // namespace ogl

#endif // !_WIN32
