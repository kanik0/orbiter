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

namespace ogl {

OGLLaunchpad::OGLLaunchpad()
{
	m_root.name = "Scenarios";
	m_root.isFolder = true;
	m_root.isOpen = true;
}

OGLLaunchpad::~OGLLaunchpad() {}

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

void OGLLaunchpad::LoadDescription(const std::string &fullPath)
{
	m_selectedDescription.clear();
	std::ifstream file(fullPath);
	if (!file.is_open()) return;

	std::string line;
	bool inDesc = false;
	while (std::getline(file, line)) {
		if (line.find("BEGIN_DESC") != std::string::npos) {
			inDesc = true;
			continue;
		}
		if (line.find("END_DESC") != std::string::npos) break;
		if (inDesc) {
			if (!m_selectedDescription.empty())
				m_selectedDescription += "\n";
			m_selectedDescription += line;
		}
	}
}

void OGLLaunchpad::RenderTree(ScenarioEntry &entry)
{
	for (auto &child : entry.children) {
		if (child.isFolder) {
			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
			if (ImGui::TreeNodeEx(child.name.c_str(), flags)) {
				RenderTree(child);
				ImGui::TreePop();
			}
		} else {
			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
			bool isSelected = (m_selectedScenario == child.path);
			if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;

			ImGui::TreeNodeEx(child.name.c_str(), flags);
			if (ImGui::IsItemClicked()) {
				m_selectedScenario = child.path;
				m_selectedScenarioFull = child.fullPath;
				LoadDescription(child.fullPath);
			}
		}
	}
}

void OGLLaunchpad::RenderTabScenario(float availH)
{
	float availW = ImGui::GetContentRegionAvail().x;
	float leftW  = availW * m_scn.splitterPos;
	if (leftW < 150.0f) leftW = 150.0f;

	ImGui::BeginChild("ScenarioTree", ImVec2(leftW, availH), true);
	RenderTree(m_root);
	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::BeginChild("ScenarioDesc", ImVec2(0, availH), true);
	if (!m_selectedScenario.empty()) {
		ImGui::TextWrapped("Scenario: %s", m_selectedScenario.c_str());
		ImGui::Separator();
		if (!m_selectedDescription.empty())
			ImGui::TextWrapped("%s", m_selectedDescription.c_str());
		else
			ImGui::TextDisabled("(no description)");
	} else {
		ImGui::TextDisabled("Select a scenario to launch");
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

	if (launch) SyncToConfig();

	return launch;
}

} // namespace ogl

#endif // !_WIN32
