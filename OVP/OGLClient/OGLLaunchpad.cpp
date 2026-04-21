// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef _WIN32

#include "OGLLaunchpad.h"
#include "Config.h"
#include "LaunchpadRegistry.h"
#include "OrbiterAPI.h"
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

// SDL2 — needed to enumerate display modes for the Video tab. SDL_Init has
// already been called by SDLPlatform before the Launchpad runs.
#include <SDL.h>

// about.hpp is generated from about.hpp.in at configure time and lives
// in the build directory. OGLClient's include path picks it up.
#include "about.hpp"

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

	// Video tab
	m_vid.bFullscreen = m_cfg->CfgDevPrm.bFullscreen;
	m_vid.bNoVsync    = m_cfg->CfgDevPrm.bNoVsync;
	m_vid.bStereo     = m_cfg->CfgDevPrm.bStereo;
	m_vid.bTryStencil = m_cfg->CfgDevPrm.bTryStencil;
	m_vid.winW        = (int)m_cfg->CfgDevPrm.WinW;
	m_vid.winH        = (int)m_cfg->CfgDevPrm.WinH;

	// Enumerate SDL display modes for the primary display, deduplicate by
	// (w,h) so the user picks a resolution rather than a refresh rate. Skip if
	// SDL_INIT_VIDEO has not been initialised — we still keep the manual
	// width/height fields working.
	m_vid.modeList.clear();
	if (SDL_WasInit(SDL_INIT_VIDEO)) {
		int nmodes = SDL_GetNumDisplayModes(0);
		for (int i = 0; i < nmodes; ++i) {
			SDL_DisplayMode dm;
			if (SDL_GetDisplayMode(0, i, &dm) != 0) continue;
			std::pair<int,int> wh{dm.w, dm.h};
			if (std::find(m_vid.modeList.begin(), m_vid.modeList.end(), wh)
				== m_vid.modeList.end())
				m_vid.modeList.push_back(wh);
		}
		// Largest first (most useful default).
		std::sort(m_vid.modeList.begin(), m_vid.modeList.end(),
			[](const std::pair<int,int>& a, const std::pair<int,int>& b) {
				return (long long)a.first * a.second > (long long)b.first * b.second;
			});
	}
	// Pick the entry that matches the saved window size. If nothing matches
	// (e.g. first-run with the compiled 800x600 default on macOS where no
	// SDL display mode is that small), fall back to the largest available
	// mode and sync winW/winH to it so the "Width"/"Height" inputs mirror
	// the Resolution combo. Without the sync the two drift out of step (#34).
	m_vid.modeIndex = -1;
	for (size_t i = 0; i < m_vid.modeList.size(); ++i) {
		if (m_vid.modeList[i].first == m_vid.winW &&
			m_vid.modeList[i].second == m_vid.winH) {
			m_vid.modeIndex = (int)i;
			break;
		}
	}
	if (m_vid.modeIndex < 0 && !m_vid.modeList.empty()) {
		m_vid.modeIndex = 0;
		m_vid.winW = m_vid.modeList[0].first;
		m_vid.winH = m_vid.modeList[0].second;
	}

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

	// Logic — only the Start-paused checkbox lives outside the Options
	// tab. All other CFG_LOGICPRM / CFG_PHYSICSPRM / CFG_VISUALPRM
	// fields are edited directly through m_cfg by the Options page
	// renderers and need no further sync.
	m_cfg->CfgLogicPrm.bStartPaused = m_startPaused;

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

	// Persist module activation state. Diff against the current Config
	// list so we add the newly checked modules and drop the unchecked
	// ones — locked modules (cmdline) stay regardless of UI state.
	if (m_mod.scanned) {
		for (const auto &m : m_mod.modules) {
			bool was = m_cfg->IsActiveModule(m.name);
			if (m.active && !was) m_cfg->AddActiveModule(m.name);
			else if (!m.active && was && !m.locked) m_cfg->DelActiveModule(m.name);
		}
	}

	// Allow every Extra item to write its addon-specific config file
	// before the simulation session starts. Mirrors Win32
	// LaunchpadDialog::WriteExtraParams (Launchpad.cpp:583).
	orbiter::LaunchpadRegistry::Instance().WriteConfigAll();
}

void OGLLaunchpad::ScanScenarios(const std::string &scenarioDir)
{
	m_root.children.clear();
	ScanDirectory(scenarioDir, "", m_root);
	m_scenariosLoaded = true;
	fprintf(stderr, "[OGLLaunchpad] Scanned scenarios from '%s'\n", scenarioDir.c_str());
}

void OGLLaunchpad::ParseModuleInfo(const std::string &infoPath, ModuleEntry &entry)
{
	std::ifstream f(infoPath);
	if (!f.is_open()) return;

	enum Section { S_NONE, S_CATEGORY, S_DESCRIPTION } sec = S_NONE;
	std::string line, desc;
	std::string cat;
	while (std::getline(f, line)) {
		if (!line.empty() && line.back() == '\r') line.pop_back();
		if (line == "[Category]")    { sec = S_CATEGORY;    continue; }
		if (line == "[Description]") { sec = S_DESCRIPTION; continue; }
		if (sec == S_CATEGORY) {
			if (cat.empty() && !line.empty()) cat = line;
		} else if (sec == S_DESCRIPTION) {
			if (!desc.empty()) desc += '\n';
			desc += line;
		}
	}
	if (!cat.empty())  entry.category    = cat;
	if (!desc.empty()) entry.description = desc;
}

void OGLLaunchpad::ScanModules(const std::string &pluginDir)
{
	m_mod.modules.clear();
	m_mod.scanned = true;
	m_mod.pluginDir = pluginDir;
	m_mod.selected = -1;

	DIR *d = opendir(pluginDir.c_str());
	if (!d) {
		fprintf(stderr, "[OGLLaunchpad] Modules/Plugin directory not found: %s\n",
			pluginDir.c_str());
		return;
	}

	struct dirent *entry;
	while ((entry = readdir(d)) != nullptr) {
		if (entry->d_name[0] == '.') continue;

		std::string fname = entry->d_name;
		// Accept Linux/macOS/Windows shared-library suffixes; strip the
		// "lib" prefix that the GNU toolchains add so the logical module
		// name matches Win32 conventions stored in Config.
		std::string base, ext;
		size_t dot = fname.rfind('.');
		if (dot == std::string::npos) continue;
		ext = fname.substr(dot);
		base = fname.substr(0, dot);
		if (ext != ".dylib" && ext != ".so" && ext != ".dll") continue;
		if (base.compare(0, 3, "lib") == 0) base = base.substr(3);

		ModuleEntry me;
		me.name     = base;
		me.filePath = pluginDir + "/" + fname;
		me.category = "Miscellaneous";

		// Look for the sibling <base>.info file written by
		// cmake/orbiter_module_info.cmake.
		ParseModuleInfo(pluginDir + "/" + base + ".info", me);

		// Seed initial activation state from the Config that was bound
		// in InitFromConfig(). We tolerate Bind() not having been called
		// yet — defaults stay false.
		if (m_cfg) {
			if (m_cfg->IsActiveModule(me.name)) me.active = true;
			for (const auto &p : m_cfg->CfgCmdlinePrm.LoadPlugins) {
				if (p == me.name) { me.active = true; me.locked = true; break; }
			}
		}
		m_mod.modules.push_back(std::move(me));
	}
	closedir(d);

	std::sort(m_mod.modules.begin(), m_mod.modules.end(),
		[](const ModuleEntry &a, const ModuleEntry &b) {
			if (a.category != b.category) return a.category < b.category;
			return a.name < b.name;
		});

	fprintf(stderr, "[OGLLaunchpad] Scanned %zu modules from '%s'\n",
		m_mod.modules.size(), pluginDir.c_str());
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
		ImGui::TextDisabled("Select a scenario to launch - double-click to start immediately.");
	}
	ImGui::EndChild();
}

// ----------------------------------------------------------------------
// Options tab — 12 pages mirroring Win32 OptionsPages.{h,cpp}.
//
// Each Render*Page function reads/writes its CFG fields directly. The
// Win32 equivalent lives in Src/Orbiter/OptionsPages.cpp; control IDs
// referenced in the Win32 sources are documented inline so a maintainer
// can locate the equivalent dialog control.
// ----------------------------------------------------------------------

namespace {

// Convenience helpers — keep call sites short.
inline bool BitFlagCheckbox(const char *label, int &flags, int bit) {
	bool v = (flags & bit) != 0;
	if (ImGui::Checkbox(label, &v)) {
		if (v) flags |= bit; else flags &= ~bit;
		return true;
	}
	return false;
}

inline bool DwordCheckbox(const char *label, DWORD *p) {
	bool v = (*p != 0);
	if (ImGui::Checkbox(label, &v)) { *p = v ? 1 : 0; return true; }
	return false;
}

void RenderVisualPage(Config *c)
{
	ImGui::TextDisabled("Surface and atmosphere");
	ImGui::Checkbox("Vessel ground shadows",   &c->CfgVisualPrm.bVesselShadows);
	ImGui::Checkbox("Surface base shadows",    &c->CfgVisualPrm.bShadows);
	ImGui::Checkbox("Cloud layers",            &c->CfgVisualPrm.bClouds);
	ImGui::Checkbox("Cloud shadows",           &c->CfgVisualPrm.bCloudShadows);
	ImGui::Checkbox("Night-side city lights",  &c->CfgVisualPrm.bNightlights);
	ImGui::Checkbox("Reflective water surface",&c->CfgVisualPrm.bWaterreflect);
	ImGui::Checkbox("Specular water ripples",  &c->CfgVisualPrm.bSpecularRipple);
	ImGui::Checkbox("Atmospheric haze",        &c->CfgVisualPrm.bHaze);
	ImGui::Checkbox("Distance fog",            &c->CfgVisualPrm.bFog);
	ImGui::Checkbox("Specular reflections",    &c->CfgVisualPrm.bSpecular);
	ImGui::Checkbox("Reentry flames",          &c->CfgVisualPrm.bReentryFlames);
	ImGui::Checkbox("Particle streams",        &c->CfgVisualPrm.bParticleStreams);
	ImGui::Checkbox("Local light sources",     &c->CfgVisualPrm.bLocalLight);

	ImGui::Spacing();
	ImGui::TextDisabled("Limits");
	int maxLight = (int)c->CfgVisualPrm.MaxLight;
	if (ImGui::SliderInt("Max light sources", &maxLight, 0, 8))
		c->CfgVisualPrm.MaxLight = (DWORD)maxLight;
	int ambient = (int)c->CfgVisualPrm.AmbientLevel;
	if (ImGui::SliderInt("Ambient light level", &ambient, 0, 255))
		c->CfgVisualPrm.AmbientLevel = (DWORD)ambient;
	int planetMaxLevel = (int)c->CfgVisualPrm.PlanetMaxLevel;
	if (ImGui::SliderInt("Planet max LOD level", &planetMaxLevel, 1, SURF_MAX_PATCHLEVEL2))
		c->CfgVisualPrm.PlanetMaxLevel = (DWORD)planetMaxLevel;
	float patchRes = (float)c->CfgVisualPrm.PlanetPatchRes;
	if (ImGui::SliderFloat("Planet patch resolution scale", &patchRes, 0.5f, 4.0f, "%.2f"))
		c->CfgVisualPrm.PlanetPatchRes = patchRes;
	float lightBright = (float)c->CfgVisualPrm.LightBrightness;
	if (ImGui::SliderFloat("Night-light brightness", &lightBright, 0.0f, 2.0f, "%.2f"))
		c->CfgVisualPrm.LightBrightness = lightBright;

	const char *elevModes[] = { "None", "Linear", "Cubic spline" };
	if (c->CfgVisualPrm.ElevMode < 0) c->CfgVisualPrm.ElevMode = 0;
	if (c->CfgVisualPrm.ElevMode > 2) c->CfgVisualPrm.ElevMode = 2;
	ImGui::Combo("Elevation interpolation", &c->CfgVisualPrm.ElevMode,
		elevModes, IM_ARRAYSIZE(elevModes));
}

void RenderPhysicsPage(Config *c)
{
	ImGui::TextDisabled("Forces and propagation");
	ImGui::Checkbox("Distributed mass model",     &c->CfgPhysicsPrm.bDistributedMass);
	ImGui::Checkbox("Non-spherical gravity",      &c->CfgPhysicsPrm.bNonsphericalGrav);
	ImGui::Checkbox("Solar radiation pressure",   &c->CfgPhysicsPrm.bRadiationPressure);
	ImGui::Checkbox("Atmospheric wind",           &c->CfgPhysicsPrm.bAtmWind);
	ImGui::Checkbox("Encke orbit stabilisation",  &c->CfgPhysicsPrm.bOrbitStabilise);

	ImGui::Spacing();
	ImGui::TextDisabled("Stabilisation thresholds");
	double pl = c->CfgPhysicsPrm.Stabilise_PLimit;
	if (ImGui::InputDouble("Perturbation limit", &pl, 0.0, 0.0, "%.4g"))
		c->CfgPhysicsPrm.Stabilise_PLimit = pl;
	double sl = c->CfgPhysicsPrm.Stabilise_SLimit;
	if (ImGui::InputDouble("Step-size limit", &sl, 0.0, 0.0, "%.4g"))
		c->CfgPhysicsPrm.Stabilise_SLimit = sl;

	ImGui::Spacing();
	ImGui::TextDisabled("Linear propagation");
	ImGui::SliderInt("Active levels", &c->CfgPhysicsPrm.nLPropLevel, 1, MAX_PROP_LEVEL);
	const char *propMethods[NPROP_METHOD] = {
		"RK2","RK4","RK5","RK6","RK7","RK8","SY2","SY4","SY6","SY8"
	};
	for (int i = 0; i < c->CfgPhysicsPrm.nLPropLevel; ++i) {
		ImGui::PushID(i);
		ImGui::Text("Level %d", i+1);
		ImGui::SameLine();
		ImGui::PushItemWidth(80.0f);
		ImGui::Combo("##m", &c->CfgPhysicsPrm.PropMode[i], propMethods, NPROP_METHOD);
		ImGui::PopItemWidth();
		ImGui::SameLine();
		ImGui::PushItemWidth(120.0f);
		ImGui::InputDouble("Δt##t", &c->CfgPhysicsPrm.PropTTgt[i], 0.0, 0.0, "%.3g");
		ImGui::SameLine();
		ImGui::InputDouble("max##l", &c->CfgPhysicsPrm.PropTLim[i], 0.0, 0.0, "%.3g");
		ImGui::PopItemWidth();
		ImGui::PopID();
	}
}

void RenderInstrumentPage(Config *c)
{
	ImGui::TextDisabled("MFD instruments");
	ImGui::SliderInt("MFD generic-view size", &c->CfgLogicPrm.MFDSize, 1, 12);
	const char *mfdMap[] = { "Legacy", "New" };
	ImGui::Combo("MFD Map style", &c->CfgLogicPrm.MFDMapVersion, mfdMap, IM_ARRAYSIZE(mfdMap));
	ImGui::Checkbox("Transparent MFD background", &c->CfgLogicPrm.bMfdTransparent);

	ImGui::Spacing();
	ImGui::TextDisabled("Instrument refresh");
	ImGui::InputDouble("Update interval [s]", &c->CfgLogicPrm.InstrUpdDT, 0.0, 0.0, "%.3f");

	ImGui::Spacing();
	ImGui::TextDisabled("Pow2 textures");
	const char *pow2Mode[] = { "No", "Yes", "Auto" };
	if (c->CfgInstrumentPrm.bMfdPow2 < 0 || c->CfgInstrumentPrm.bMfdPow2 > 2)
		c->CfgInstrumentPrm.bMfdPow2 = 2;
	ImGui::Combo("Force pow2 MFD textures", &c->CfgInstrumentPrm.bMfdPow2,
		pow2Mode, IM_ARRAYSIZE(pow2Mode));
	ImGui::SliderInt("Pow2 hi-res threshold", &c->CfgInstrumentPrm.MfdHiresThreshold, 64, 1024);
	const char *texSizes[] = { "256", "512", "1024", "2048" };
	auto sizeIdx = [](int v){
		switch (v) { case 256: return 0; case 512: return 1;
			case 1024: return 2; case 2048: return 3; }
		return 1;
	};
	auto idxSize = [](int i){
		const int s[] = {256, 512, 1024, 2048}; return s[i];
	};
	int panelIdx = sizeIdx(c->CfgInstrumentPrm.PanelMFDHUDSize);
	if (ImGui::Combo("2D panel MFD/HUD size", &panelIdx, texSizes, IM_ARRAYSIZE(texSizes)))
		c->CfgInstrumentPrm.PanelMFDHUDSize = idxSize(panelIdx);
	int vcIdx = sizeIdx(c->CfgInstrumentPrm.VCMFDSize);
	if (ImGui::Combo("Virtual cockpit MFD size", &vcIdx, texSizes, IM_ARRAYSIZE(texSizes)))
		c->CfgInstrumentPrm.VCMFDSize = idxSize(vcIdx);

	ImGui::Spacing();
	ImGui::TextDisabled("2D panel layout");
	float scale = (float)c->CfgLogicPrm.PanelScale;
	if (ImGui::SliderFloat("Panel scale", &scale, 0.25f, 4.0f, "%.2f"))
		c->CfgLogicPrm.PanelScale = scale;
	float scrollSpeed = (float)c->CfgLogicPrm.PanelScrollSpeed;
	if (ImGui::SliderFloat("Panel scroll speed [px/s]", &scrollSpeed, 50.0f, 1000.0f, "%.0f"))
		c->CfgLogicPrm.PanelScrollSpeed = scrollSpeed;
	ImGui::Checkbox("Compact glass cockpit (widescreen)", &c->CfgLogicPrm.bGlasspitCompact);
}

void RenderVesselPage(Config *c)
{
	ImGui::TextDisabled("Flight model");
	const char *flight[] = { "Simplified", "Realistic" };
	ImGui::Combo("Flight model fidelity", &c->CfgLogicPrm.FlightModelLevel,
		flight, IM_ARRAYSIZE(flight));
	const char *damage[] = { "No damage model", "Allow damage" };
	ImGui::Combo("Damage model", &c->CfgLogicPrm.DamageSetting,
		damage, IM_ARRAYSIZE(damage));

	ImGui::Spacing();
	ImGui::TextDisabled("Fuel and refuelling");
	ImGui::Checkbox("Limited fuel",         &c->CfgLogicPrm.bLimitedFuel);
	ImGui::Checkbox("Auto-refuel on pad",   &c->CfgLogicPrm.bPadRefuel);
}

void RenderUIPage(Config *c)
{
	ImGui::TextDisabled("Mouse focus");
	const char *focus[] = { "Click required", "Click for child", "Follow mouse" };
	ImGui::Combo("Window focus mode", &c->CfgUIPrm.MouseFocusMode,
		focus, IM_ARRAYSIZE(focus));

	ImGui::Spacing();
	ImGui::TextDisabled("Menubar / Infobar");
	const char *visMode[] = { "Show", "Hide", "Auto-hide" };
	ImGui::Combo("Menubar mode", &c->CfgUIPrm.MenuMode, visMode, IM_ARRAYSIZE(visMode));
	ImGui::Combo("Infobar mode", &c->CfgUIPrm.InfoMode, visMode, IM_ARRAYSIZE(visMode));
	ImGui::Checkbox("Always show menu labels", &c->CfgUIPrm.bMenuLabelAlways);
	ImGui::Checkbox("Always show time-warp",   &c->CfgUIPrm.bWarpAlways);

	ImGui::SliderInt("Menubar opacity (0-10)", &c->CfgUIPrm.MenuOpacity, 0, 10);
	ImGui::SliderInt("Infobar opacity (0-20)", &c->CfgUIPrm.InfoOpacity, 0, 20);
	ImGui::SliderInt("Menubar scroll speed",   &c->CfgUIPrm.MenuScrollspeed, 1, 20);
	ImGui::SliderInt("Menu button size",        &c->CfgUIPrm.MenuButtonSize, 12, 64);
	ImGui::SliderInt("Menu button hover size",  &c->CfgUIPrm.MenuButtonHoverSize, 12, 96);
	ImGui::SliderInt("Menu button spacing",     &c->CfgUIPrm.MenuButtonSpacing, 0, 32);

	ImGui::Spacing();
	ImGui::TextDisabled("FPS overlay");
	const char *fpsMode[] = { "Hidden", "Left", "Right" };
	ImGui::Combo("FPS counter", &c->CfgUIPrm.FPS, fpsMode, IM_ARRAYSIZE(fpsMode));
}

void RenderJoystickPage(Config *c)
{
	ImGui::TextDisabled("Device");
	int joyIdx = (int)c->CfgJoystickPrm.Joy_idx;
	if (ImGui::InputInt("Joystick index (0 = disabled)", &joyIdx)) {
		if (joyIdx < 0) joyIdx = 0;
		c->CfgJoystickPrm.Joy_idx = (DWORD)joyIdx;
	}

	ImGui::Spacing();
	ImGui::TextDisabled("Calibration");
	ImGui::SliderInt("Central deadzone (0-10000)",
		&c->CfgJoystickPrm.Deadzone, 0, 10000);
	const char *throttleAxis[] = { "None", "Z-axis", "Slider 0", "Slider 1" };
	int axisIdx = (int)c->CfgJoystickPrm.ThrottleAxis;
	if (axisIdx < 0 || axisIdx >= IM_ARRAYSIZE(throttleAxis)) axisIdx = 0;
	if (ImGui::Combo("Throttle axis", &axisIdx, throttleAxis, IM_ARRAYSIZE(throttleAxis)))
		c->CfgJoystickPrm.ThrottleAxis = (DWORD)axisIdx;
	ImGui::SliderInt("Throttle saturation (0-10000)",
		&c->CfgJoystickPrm.ThrottleSaturation, 0, 10000);
	ImGui::Checkbox("Ignore initial throttle position",
		&c->CfgJoystickPrm.bThrottleIgnore);

	ImGui::Spacing();
	ImGui::TextDisabled("Haptic feedback");
	ImGui::SliderFloat("Rumble gain (0 disables)",
		&c->CfgJoystickPrm.HapticGain, 0.0f, 2.0f, "%.2f");
}

void RenderCelSpherePage(Config *c)
{
	ImGui::TextDisabled("Stars");
	ImGui::Checkbox("Render as pixels",  &c->CfgVisualPrm.bUseStarDots);
	ImGui::Checkbox("Render as image",   &c->CfgVisualPrm.bUseStarImage);
	ImGui::InputText("Starlist image path",
		c->CfgVisualPrm.StarImagePath, sizeof(c->CfgVisualPrm.StarImagePath));

	ImGui::Spacing();
	ImGui::TextDisabled("Background image");
	ImGui::Checkbox("Render background", &c->CfgVisualPrm.bUseBgImage);
	ImGui::InputText("Background path",
		c->CfgVisualPrm.CSphereBgPath, sizeof(c->CfgVisualPrm.CSphereBgPath));
	float bgI = (float)c->CfgVisualPrm.CSphereBgIntens;
	if (ImGui::SliderFloat("Background intensity", &bgI, 0.0f, 2.0f, "%.2f"))
		c->CfgVisualPrm.CSphereBgIntens = bgI;

	ImGui::Spacing();
	ImGui::TextDisabled("Star magnitude mapping");
	float minMag = c->CfgVisualPrm.StarPrm.mag_lo;
	float maxMag = c->CfgVisualPrm.StarPrm.mag_hi;
	if (ImGui::SliderFloat("Min magnitude (faintest)", &minMag, -2.0f, 12.0f, "%.2f"))
		c->CfgVisualPrm.StarPrm.mag_lo = minMag;
	if (ImGui::SliderFloat("Max magnitude (brightest)", &maxMag, -2.0f, 12.0f, "%.2f"))
		c->CfgVisualPrm.StarPrm.mag_hi = maxMag;
	float br = c->CfgVisualPrm.StarPrm.brt_min;
	if (ImGui::SliderFloat("Min star brightness", &br, 0.0f, 1.0f, "%.2f"))
		c->CfgVisualPrm.StarPrm.brt_min = br;
	const char *map[] = { "Linear", "Exponential" };
	int curMap = c->CfgVisualPrm.StarPrm.map_log ? 1 : 0;
	if (ImGui::Combo("Magnitude→brightness mapping", &curMap, map, IM_ARRAYSIZE(map)))
		c->CfgVisualPrm.StarPrm.map_log = (curMap != 0);
}

void RenderVisHelperPage(Config *c)
{
	ImGui::TextDisabled("Visual helpers (Ctrl-F9 in flight)");
	BitFlagCheckbox("Planetarium mode",  c->CfgVisHelpPrm.flagPlanetarium, 0x0001);
	BitFlagCheckbox("Surface markers",   c->CfgVisHelpPrm.flagMarkers, 0x0001);
	ImGui::Separator();
	ImGui::TextWrapped("Detail toggles for grids, constellations, body markers, force "
		"vectors and frame axes are on the dedicated sub-pages.");
}

void RenderPlanetariumPage(Config *c)
{
	ImGui::TextDisabled("Planetarium overlay (bitfield)");
	int &f = c->CfgVisHelpPrm.flagPlanetarium;
	BitFlagCheckbox("Enabled",                  f, 0x0001);
	BitFlagCheckbox("Celestial grid",           f, 0x0002);
	BitFlagCheckbox("Ecliptic grid",            f, 0x0004);
	BitFlagCheckbox("Galactic grid",            f, 0x0008);
	BitFlagCheckbox("Equator of current target",f, 0x0010);
	BitFlagCheckbox("Constellation patterns",   f, 0x0020);
	BitFlagCheckbox("Constellation labels",     f, 0x0040);
	BitFlagCheckbox("Long constellation names", f, 0x0080);
	BitFlagCheckbox("Constellation boundaries", f, 0x0100);
	BitFlagCheckbox("Celestial sphere markers", f, 0x0200);
}

void RenderLabelsPage(Config *c)
{
	ImGui::TextDisabled("Surface and object markers (bitfield)");
	int &f = c->CfgVisHelpPrm.flagMarkers;
	BitFlagCheckbox("Enabled",                f, 0x0001);
	BitFlagCheckbox("Solar-system bodies",    f, 0x0002);
	BitFlagCheckbox("Vessels",                f, 0x0004);
	BitFlagCheckbox("Surface bases",          f, 0x0008);
	BitFlagCheckbox("VOR transmitters",       f, 0x0010);
	BitFlagCheckbox("Surface features",       f, 0x0020);
}

void RenderForcesPage(Config *c)
{
	ImGui::TextDisabled("Body force vectors (bitfield)");
	int &f = c->CfgVisHelpPrm.flagBodyForce;
	BitFlagCheckbox("Enabled",         f, 0x0001);
	BitFlagCheckbox("Weight",          f, 0x0002);
	BitFlagCheckbox("Thrust",          f, 0x0004);
	BitFlagCheckbox("Lift",            f, 0x0008);
	BitFlagCheckbox("Drag",            f, 0x0010);
	BitFlagCheckbox("Total",           f, 0x0020);
	BitFlagCheckbox("Torque",          f, 0x0040);
	BitFlagCheckbox("Linear / per-N",  f, 0x0080);

	ImGui::Spacing();
	ImGui::SliderFloat("Vector scale",   &c->CfgVisHelpPrm.scaleBodyForce, 0.1f, 5.0f, "%.2f");
	ImGui::SliderFloat("Vector opacity", &c->CfgVisHelpPrm.opacBodyForce, 0.0f, 1.0f, "%.2f");
}

void RenderAxesPage(Config *c)
{
	ImGui::TextDisabled("Object frame axes (bitfield)");
	int &f = c->CfgVisHelpPrm.flagFrameAxes;
	BitFlagCheckbox("Enabled",        f, 0x0001);
	BitFlagCheckbox("Vessels",        f, 0x0002);
	BitFlagCheckbox("Celestial bodies", f, 0x0004);
	BitFlagCheckbox("Surface bases",  f, 0x0008);
	BitFlagCheckbox("Negative axes",  f, 0x0010);

	ImGui::Spacing();
	ImGui::SliderFloat("Axis scale",   &c->CfgVisHelpPrm.scaleFrameAxes, 0.1f, 5.0f, "%.2f");
	ImGui::SliderFloat("Axis opacity", &c->CfgVisHelpPrm.opacFrameAxes, 0.0f, 1.0f, "%.2f");
}

} // namespace

void OGLLaunchpad::RenderTabOptions(float availH)
{
	ImGui::BeginChild("OptionsContent", ImVec2(0, availH), false);

	if (!m_cfg) {
		ImGui::TextDisabled("Config not bound - Options unavailable.");
		ImGui::EndChild();
		return;
	}

	struct Page { const char *label; int idx; };
	static const Page pages[] = {
		{"Visual",         0},
		{"Physics",        1},
		{"Instruments",    2},
		{"Vessel",         3},
		{"User interface", 4},
		{"Joystick",       5},
		{"Celestial sphere", 6},
		// ASCII indent: U+2514 (└) isn't in the Lekton glyph coverage,
		// so the Options sidebar rendered '?' next to each Visual-helpers
		// child (#31). Plain spaces + a pipe keep the hierarchy readable
		// without relying on line-drawing characters.
		{"Visual helpers",     7},
		{"    Planetarium",    8},
		{"    Labels",         9},
		{"    Forces",        10},
		{"    Frame axes",    11},
	};

	float availW = ImGui::GetContentRegionAvail().x;
	float listW  = availW * m_opt.splitterPos;
	if (listW < 160.0f) listW = 160.0f;

	ImGui::BeginChild("OptPages", ImVec2(listW, availH), true);
	for (auto &p : pages) {
		bool sel = (m_opt.currentPage == p.idx);
		if (ImGui::Selectable(p.label, sel))
			m_opt.currentPage = p.idx;
	}
	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::BeginChild("OptForm", ImVec2(0, availH), true);
	switch (m_opt.currentPage) {
		case 0:  RenderVisualPage(m_cfg);     break;
		case 1:  RenderPhysicsPage(m_cfg);    break;
		case 2:  RenderInstrumentPage(m_cfg); break;
		case 3:  RenderVesselPage(m_cfg);     break;
		case 4:  RenderUIPage(m_cfg);         break;
		case 5:  RenderJoystickPage(m_cfg);   break;
		case 6:  RenderCelSpherePage(m_cfg);  break;
		case 7:  RenderVisHelperPage(m_cfg);  break;
		case 8:  RenderPlanetariumPage(m_cfg);break;
		case 9:  RenderLabelsPage(m_cfg);     break;
		case 10: RenderForcesPage(m_cfg);     break;
		case 11: RenderAxesPage(m_cfg);       break;
	}
	ImGui::EndChild();

	ImGui::EndChild();
}

void OGLLaunchpad::RenderTabModules(float availH)
{
	ImGui::BeginChild("ModulesContent", ImVec2(0, availH), false);

	if (!m_mod.scanned) {
		ImGui::TextDisabled("Modules/Plugin/ has not been scanned yet.");
		ImGui::EndChild();
		return;
	}

	const float footerH = ImGui::GetFrameHeightWithSpacing() + 6.0f;
	float listH = availH - footerH;
	if (listH < 60.0f) listH = 60.0f;

	float listW = ImGui::GetContentRegionAvail().x * 0.5f;
	if (listW < 200.0f) listW = 200.0f;

	// Left pane: categorised tree of plugins with checkboxes.
	ImGui::BeginChild("ModList", ImVec2(listW, listH), true);
	if (m_mod.modules.empty()) {
		ImGui::TextDisabled("(no plugin modules found)");
	} else {
		std::string lastCat;
		bool catOpen = false;
		for (size_t i = 0; i < m_mod.modules.size(); ++i) {
			ModuleEntry &m = m_mod.modules[i];
			if (m.category != lastCat) {
				if (!lastCat.empty() && catOpen) ImGui::Unindent();
				lastCat = m.category;
				catOpen = ImGui::CollapsingHeader(m.category.c_str(),
					ImGuiTreeNodeFlags_DefaultOpen);
				if (catOpen) ImGui::Indent();
			}
			if (!catOpen) continue;

			ImGui::PushID((int)i);
			bool active = m.active;
			bool disabled = m.locked;
			if (disabled) ImGui::BeginDisabled();
			if (ImGui::Checkbox(m.name.c_str(), &active) && !disabled)
				m.active = active;
			if (disabled) ImGui::EndDisabled();
			if (ImGui::IsItemClicked())
				m_mod.selected = (int)i;
			ImGui::PopID();
		}
		if (catOpen) ImGui::Unindent();
	}
	ImGui::EndChild();

	ImGui::SameLine();

	// Right pane: description of the selected module.
	ImGui::BeginChild("ModDesc", ImVec2(0, listH), true);
	if (m_mod.selected >= 0 && m_mod.selected < (int)m_mod.modules.size()) {
		const ModuleEntry &m = m_mod.modules[m_mod.selected];
		ImGui::TextWrapped("Module: %s", m.name.c_str());
		ImGui::Text("Category: %s", m.category.c_str());
		if (m.locked) ImGui::TextColored(ImVec4(1,0.7f,0.3f,1),
			"(locked active by --plugin command-line flag)");
		ImGui::Separator();
		if (!m.description.empty())
			ImGui::TextWrapped("%s", m.description.c_str());
		else
			ImGui::TextDisabled("(no description - module ships without a .info sidecar)");
	} else {
		ImGui::TextDisabled("Select a module to see its description.\n\n"
			"Optional Orbiter plugin modules.\n"
			"Check or uncheck items to activate the corresponding modules.");
	}
	ImGui::EndChild();

	// Footer: action buttons.
	if (ImGui::Button("Deactivate all")) {
		for (auto &m : m_mod.modules) if (!m.locked) m.active = false;
	}
	ImGui::SameLine();
	if (ImGui::Button("Rescan")) ScanModules(m_mod.pluginDir);

	ImGui::EndChild();
}

void OGLLaunchpad::RenderTabVideo(float availH)
{
	ImGui::BeginChild("VideoContent", ImVec2(0, availH), false);

	ImGui::TextDisabled("Graphics engine");
	ImGui::Text("OGLClient (OpenGL 4.1 Core Profile, SDL2)");
	ImGui::Text("Renderer: built-in / single client on macOS");
	ImGui::Separator();
	ImGui::Spacing();

	ImGui::TextDisabled("Display");

	// Resolution combo populated from SDL_GetDisplayMode.
	if (!m_vid.modeList.empty()) {
		std::string preview;
		if (m_vid.modeIndex >= 0 && m_vid.modeIndex < (int)m_vid.modeList.size()) {
			char buf[64];
			snprintf(buf, sizeof(buf), "%d x %d",
				m_vid.modeList[m_vid.modeIndex].first,
				m_vid.modeList[m_vid.modeIndex].second);
			preview = buf;
		} else {
			preview = "(custom)";
		}
		if (ImGui::BeginCombo("Resolution", preview.c_str())) {
			for (size_t i = 0; i < m_vid.modeList.size(); ++i) {
				char buf[64];
				snprintf(buf, sizeof(buf), "%d x %d",
					m_vid.modeList[i].first, m_vid.modeList[i].second);
				bool sel = ((int)i == m_vid.modeIndex);
				if (ImGui::Selectable(buf, sel)) {
					m_vid.modeIndex = (int)i;
					m_vid.winW = m_vid.modeList[i].first;
					m_vid.winH = m_vid.modeList[i].second;
				}
				if (sel) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
	} else {
		ImGui::TextDisabled("(SDL display modes unavailable - using manual size)");
	}

	// Manual width / height for users who need an off-list size. After any
	// manual edit, re-match against the mode list so the Resolution combo
	// either snaps to the matching entry or shows "(custom)" rather than
	// drifting stale behind the inputs (#34 acceptance criterion).
	ImGui::PushItemWidth(120.0f);
	bool winEdited = false;
	if (ImGui::InputInt("Width",  &m_vid.winW, 0)) {
		if (m_vid.winW < 320) m_vid.winW = 320;
		winEdited = true;
	}
	ImGui::SameLine();
	if (ImGui::InputInt("Height", &m_vid.winH, 0)) {
		if (m_vid.winH < 240) m_vid.winH = 240;
		winEdited = true;
	}
	ImGui::PopItemWidth();
	if (winEdited) {
		m_vid.modeIndex = -1;
		for (size_t i = 0; i < m_vid.modeList.size(); ++i) {
			if (m_vid.modeList[i].first == m_vid.winW &&
				m_vid.modeList[i].second == m_vid.winH) {
				m_vid.modeIndex = (int)i;
				break;
			}
		}
	}

	ImGui::Spacing();
	ImGui::TextDisabled("Mode");
	ImGui::Checkbox("Fullscreen",          &m_vid.bFullscreen);
	bool vsync = !m_vid.bNoVsync;
	if (ImGui::Checkbox("Vertical sync",   &vsync))
		m_vid.bNoVsync = !vsync;
	ImGui::Checkbox("Try stencil buffer",  &m_vid.bTryStencil);
	ImGui::Checkbox("Stereo (anaglyph)",   &m_vid.bStereo);

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::TextWrapped("Changes apply on the next launch. macOS uses "
		"borderless fullscreen - the OS picks the optimum refresh rate.");

	ImGui::EndChild();
}

// Recursive renderer for the Extra tree. Draws every entry whose
// `parent` matches `parentH`, recurses into children when the user
// expands the node, and tracks selection in m_ext.selectedItem.
static void RenderExtraTree(const std::vector<orbiter::LaunchpadEntry> &entries,
	orbiter::LpadHandle parentH, ExtraTabState &state,
	orbiter::LpadHandle &openItem)
{
	for (size_t i = 0; i < entries.size(); ++i) {
		const auto &e = entries[i];
		if (e.parent != parentH) continue;

		const char *name = e.item->Name();
		if (!name || !*name) name = "(unnamed)";

		// Check whether this entry has any children to decide between
		// a leaf node and an expandable folder.
		bool hasChildren = false;
		for (const auto &c : entries) {
			if (c.parent == e.handle) { hasChildren = true; break; }
		}

		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
		if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf
			| ImGuiTreeNodeFlags_NoTreePushOnOpen;
		if (state.selectedItem == (int)e.handle)
			flags |= ImGuiTreeNodeFlags_Selected;

		bool nodeOpen = ImGui::TreeNodeEx((const void*)(intptr_t)e.handle,
			flags, "%s", name);
		if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
			state.selectedItem = (int)e.handle;
		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
			openItem = e.handle;

		if (nodeOpen && hasChildren) {
			RenderExtraTree(entries, e.handle, state, openItem);
			ImGui::TreePop();
		}
	}
}

void OGLLaunchpad::RenderTabExtra(float availH)
{
	ImGui::BeginChild("ExtraContent", ImVec2(0, availH), false);

	const auto &entries = orbiter::LaunchpadRegistry::Instance().Entries();
	if (entries.empty()) {
		ImGui::TextDisabled("No additional parameters registered.\n\n"
			"Built-in items (Physics, Instruments, Debug, ...) are added at "
			"Orbiter startup. Plugin modules can register their own items via "
			"oapiRegisterLaunchpadItem().");
		ImGui::EndChild();
		return;
	}

	float availW = ImGui::GetContentRegionAvail().x;
	float leftW  = availW * m_ext.splitterPos;
	if (leftW < 180.0f) leftW = 180.0f;

	orbiter::LpadHandle openItem = 0;

	ImGui::BeginChild("ExtTree", ImVec2(leftW, availH), true);
	RenderExtraTree(entries, 0, m_ext, openItem);
	ImGui::EndChild();

	VerticalSplitter("##ExtSplit", availW, availH, m_ext.splitterPos, m_draggingScnSplitter);
	ImGui::SameLine();

	ImGui::BeginChild("ExtDesc", ImVec2(0, availH), true);
	const orbiter::LaunchpadEntry *sel = nullptr;
	for (const auto &e : entries) {
		if ((int)e.handle == m_ext.selectedItem) { sel = &e; break; }
	}
	if (sel) {
		const char *name = sel->item->Name(); if (!name) name = "(unnamed)";
		ImGui::TextWrapped("%s", name);
		ImGui::Separator();
		const char *desc = sel->item->Description();
		if (desc && *desc)
			ImGui::TextWrapped("%s", desc);
		else
			ImGui::TextDisabled("(no description)");
		ImGui::Spacing();
		if (ImGui::Button("Edit...")) openItem = sel->handle;
	} else {
		ImGui::TextDisabled("Select an item to see its description.\n"
			"Double-click an item (or use the Edit button) to open its editor.");
	}
	ImGui::EndChild();

	// Modal popup for the selected item's clbkRender(). The popup stays
	// alive while the item's clbkRender() returns true.
	static orbiter::LpadHandle activeItem = 0;
	static LaunchpadItem *activePtr = nullptr;
	if (openItem) {
		for (const auto &e : entries) {
			if (e.handle == openItem) {
				activeItem = openItem;
				activePtr = e.item;
				ImGui::OpenPopup("##ExtraEdit");
				break;
			}
		}
	}
	ImVec2 vp = ImGui::GetMainViewport()->Size;
	ImGui::SetNextWindowSize(ImVec2(vp.x * 0.5f, vp.y * 0.6f), ImGuiCond_Appearing);
	ImGui::SetNextWindowPos(ImVec2(vp.x * 0.5f, vp.y * 0.5f),
		ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if (ImGui::BeginPopupModal("##ExtraEdit", nullptr, ImGuiWindowFlags_None)) {
		const char *name = activePtr ? activePtr->Name() : nullptr;
		ImGui::TextUnformatted(name && *name ? name : "Edit");
		ImGui::Separator();

		bool keepOpen = activePtr ? activePtr->clbkRender() : false;

		ImGui::Separator();
		if (ImGui::Button("Close")) keepOpen = false;
		if (!keepOpen) {
			ImGui::CloseCurrentPopup();
			activeItem = 0;
			activePtr  = nullptr;
		}
		ImGui::EndPopup();
	}

	ImGui::EndChild();
}

// Open a URL in the platform browser. macOS goes through `open`; falls
// back to no-op silently if the system command is missing so the
// Launchpad never crashes from a click.
static void OpenURL(const char *url)
{
	char cmd[1024];
	snprintf(cmd, sizeof(cmd), "open '%s' >/dev/null 2>&1 &", url);
	if (std::system(cmd) != 0) {
		fprintf(stderr, "[OGLLaunchpad] Failed to open URL: %s\n", url);
	}
}

void OGLLaunchpad::RenderTabAbout(float availH)
{
	ImGui::BeginChild("AboutContent", ImVec2(0, availH), false);
	ImGui::Spacing();

	ImGui::PushFont(nullptr); // default font; left for future bigger title font
	ImGui::Text("%s", NAME1);
	ImGui::PopFont();

	ImGui::Text("%s", SIG4);
	ImGui::Text("%s", SIG1B);
	ImGui::Spacing();
	ImGui::TextDisabled("macOS / Apple Silicon port");
	ImGui::Text("OpenGL Client: OGLClient (OpenGL 4.1 Core Profile + SDL2 + ImGui)");

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	ImGui::TextDisabled("Components");
	ImGui::BulletText("D3D9Client (Win32 reference) - Jarmo Nikkanen, Peter Schneider");
	ImGui::BulletText("XRSound - Doug Beachy");
	ImGui::BulletText("ImGui - Omar Cornut & contributors");
	ImGui::BulletText("OpenAL Soft - Chris Robinson");
	ImGui::BulletText("stb_image - Sean Barrett");
	ImGui::BulletText("SDL2 - Sam Lantinga & contributors");

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	ImGui::TextDisabled("Online resources");
	if (ImGui::Button("Project homepage")) OpenURL("http://" SIG2);
	ImGui::SameLine();
	if (ImGui::Button("Forum"))            OpenURL("https://" SIG5);
	ImGui::SameLine();
	if (ImGui::Button("YouTube channel")) OpenURL("https://" SIG6);

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	ImGui::TextWrapped("Orbiter is a realistic spaceflight simulation that lets you "
		"experience manned and unmanned space missions from the pilot's perspective. "
		"Free, non-commercial, distributed without warranty.");

	ImGui::EndChild();
}

bool OGLLaunchpad::Render(bool &quit)
{
	quit = false;
	bool launch = false;

	ImGuiViewport *viewport = ImGui::GetMainViewport();
	ImVec2 center = viewport->GetCenter();

	// Restore previous Launchpad geometry from Config (encoded as the
	// Win32 RECT rLaunchpad). Falls back to a centred 70%×80% layout
	// the first time the user runs Orbiter.
	float winW, winH, winX, winY;
	bool haveSaved = false;
	if (m_cfg) {
		const RECT &r = m_cfg->rLaunchpad;
		if (r.right > r.left && r.bottom > r.top) {
			winW = (float)(r.right - r.left);
			winH = (float)(r.bottom - r.top);
			winX = (float)r.left;
			winY = (float)r.top;
			haveSaved = true;
		}
	}
	if (!haveSaved) {
		winW = viewport->Size.x * 0.7f;
		winH = viewport->Size.y * 0.8f;
		if (winW < 700) winW = 700;
		if (winH < 500) winH = 500;
		winX = center.x - winW * 0.5f;
		winY = center.y - winH * 0.5f;
	}

	ImGui::SetNextWindowPos(ImVec2(winX, winY), ImGuiCond_Once);
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

		// Tooltip + explicit style override on top of BeginDisabled() —
		// the default DisabledAlpha (0.6) left the button nearly
		// indistinguishable from the active state against our theme (#30).
		bool canLaunch = !m_selectedScenario.empty();
		if (!canLaunch) {
			ImGui::BeginDisabled();
			ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.25f, 0.25f, 0.28f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.28f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.25f, 0.25f, 0.28f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.55f, 0.55f, 0.58f, 1.0f));
		}
		if (ImGui::Button("Launch Orbiter", ImVec2(bw, 0)))
			launch = true;
		if (!canLaunch) {
			ImGui::PopStyleColor(4);
			ImGui::EndDisabled();
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				ImGui::SetTooltip("Select a scenario from the list to enable launch.");
		}

		ImGui::SameLine();
		if (ImGui::Button("Exit", ImVec2(bw, 0)))
			quit = true;

		// Capture current window geometry while the window is still
		// the active context, so we can restore it on the next launch.
		ImVec2 lpadPos  = ImGui::GetWindowPos();
		ImVec2 lpadSize = ImGui::GetWindowSize();
		m_lastLpadX = lpadPos.x;
		m_lastLpadY = lpadPos.y;
		m_lastLpadW = lpadSize.x;
		m_lastLpadH = lpadSize.y;
	}
	ImGui::End();

	if (!open) quit = true;

	if (s_pendingLaunch && !m_selectedScenario.empty()) {
		s_pendingLaunch = false;
		launch = true;
	}

	if ((launch || quit) && m_cfg) {
		// Persist the geometry into rLaunchpad (Win32 RECT-encoded);
		// the loader uses the same struct on next start.
		m_cfg->rLaunchpad.left   = (LONG)m_lastLpadX;
		m_cfg->rLaunchpad.top    = (LONG)m_lastLpadY;
		m_cfg->rLaunchpad.right  = (LONG)(m_lastLpadX + m_lastLpadW);
		m_cfg->rLaunchpad.bottom = (LONG)(m_lastLpadY + m_lastLpadH);
	}

	if (launch) SyncToConfig();

	return launch;
}

} // namespace ogl

#endif // !_WIN32
