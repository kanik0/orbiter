// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef _WIN32

#include "OGLLaunchpad.h"
#include "imgui.h"
#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>
#include <fstream>
#include <cstring>
#include <cstdio>

namespace ogl {

OGLLaunchpad::OGLLaunchpad()
	: m_startPaused(false), m_scenariosLoaded(false)
{
	m_root.name = "Scenarios";
	m_root.isFolder = true;
	m_root.isOpen = true;
}

OGLLaunchpad::~OGLLaunchpad() {}

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
			// Check for .scn extension
			size_t extPos = name.rfind(".scn");
			if (extPos == std::string::npos) continue;
			if (extPos + 4 != name.size()) continue;

			ScenarioEntry se;
			se.name = name.substr(0, extPos); // strip .scn
			se.path = rel.substr(0, rel.size() - 4); // strip .scn from path
			se.fullPath = fullPath;
			se.isFolder = false;
			se.isOpen = false;
			files.push_back(std::move(se));
		}
	}
	closedir(d);

	// Sort alphabetically
	std::sort(folders.begin(), folders.end(), [](const ScenarioEntry &a, const ScenarioEntry &b) {
		return a.name < b.name;
	});
	std::sort(files.begin(), files.end(), [](const ScenarioEntry &a, const ScenarioEntry &b) {
		return a.name < b.name;
	});

	// Folders first, then files
	for (auto &f : folders) parent.children.push_back(std::move(f));
	for (auto &f : files) parent.children.push_back(std::move(f));
}

void OGLLaunchpad::LoadDescription(const std::string &fullPath)
{
	m_selectedDescription.clear();
	std::ifstream file(fullPath);
	if (!file.is_open()) return;

	// Read the BEGIN_DESC / END_DESC block from the .scn file
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
				LoadDescription(child.fullPath);
			}
			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
				m_selectedScenario = child.path;
				// Double-click = launch
			}
		}
	}
}

bool OGLLaunchpad::Render(bool &quit)
{
	quit = false;
	bool launch = false;

	ImGuiViewport *viewport = ImGui::GetMainViewport();
	ImVec2 center = viewport->GetCenter();

	// Window size: ~60% of viewport
	float winW = viewport->Size.x * 0.6f;
	float winH = viewport->Size.y * 0.7f;
	if (winW < 600) winW = 600;
	if (winH < 400) winH = 400;

	ImGui::SetNextWindowPos(ImVec2(center.x - winW * 0.5f, center.y - winH * 0.5f), ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(winW, winH), ImGuiCond_Once);

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;
	bool open = true;
	if (ImGui::Begin("Orbiter Space Flight Simulator", &open, flags)) {

		if (ImGui::BeginTabBar("LaunchpadTabs")) {

			// === Scenario Tab ===
			if (ImGui::BeginTabItem("Scenarios")) {
				float availH = ImGui::GetContentRegionAvail().y - 80;

				// Left: scenario tree
				ImGui::BeginChild("ScenarioTree", ImVec2(winW * 0.45f, availH), true);
				RenderTree(m_root);
				ImGui::EndChild();

				ImGui::SameLine();

				// Right: description
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

				ImGui::Separator();
				ImGui::Checkbox("Start paused", &m_startPaused);
				ImGui::SameLine(winW - 200);

				bool canLaunch = !m_selectedScenario.empty();
				if (!canLaunch) ImGui::BeginDisabled();
				if (ImGui::Button("Launch Orbiter", ImVec2(160, 36)))
					launch = true;
				if (!canLaunch) ImGui::EndDisabled();

				ImGui::EndTabItem();
			}

			// === Options Tab ===
			if (ImGui::BeginTabItem("Options")) {
				ImGui::TextDisabled("Simulation options (not yet implemented)");
				ImGui::EndTabItem();
			}

			// === Modules Tab ===
			if (ImGui::BeginTabItem("Modules")) {
				ImGui::TextDisabled("Module management (not yet implemented)");
				ImGui::EndTabItem();
			}

			// === About Tab ===
			if (ImGui::BeginTabItem("About")) {
				ImGui::Spacing();
				ImGui::Text("Orbiter Space Flight Simulator");
				ImGui::Text("macOS / Apple Silicon Port");
				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Spacing();
				ImGui::Text("Original: Martin Schweiger");
				ImGui::Text("OpenGL Client: OGLClient (OpenGL 4.1 + SDL2)");
				ImGui::Spacing();
				ImGui::TextWrapped("Orbiter is a realistic spaceflight simulation that lets you "
					"experience manned and unmanned space missions from the pilot's perspective.");
				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}
	}
	ImGui::End();

	if (!open) quit = true;

	return launch;
}

} // namespace ogl

#endif // !_WIN32
