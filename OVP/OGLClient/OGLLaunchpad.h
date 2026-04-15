// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// OGLLaunchpad - ImGui-based pre-simulation launchpad dialog for macOS/Linux
// Provides scenario selection, launch control, and basic options

#ifndef __OGLLLAUNCHPAD_H
#define __OGLLLAUNCHPAD_H

#ifndef _WIN32

#include <string>
#include <vector>
#include <functional>

namespace ogl {

// Tree entry for scenario browser
struct ScenarioEntry {
	std::string name;        // display name (filename without .scn)
	std::string path;        // relative path for Launch() (e.g., "Demo/Today")
	std::string fullPath;    // full filesystem path
	bool isFolder;
	bool isOpen;             // ImGui tree state
	std::vector<ScenarioEntry> children;
};

class OGLLaunchpad {
public:
	OGLLaunchpad();
	~OGLLaunchpad();

	// Scan the Scenarios/ directory and build the tree
	void ScanScenarios(const std::string &scenarioDir);

	// Render one frame of the launchpad UI. Returns:
	//   true  = user clicked Launch (selectedScenario is set)
	//   false = still browsing (call again next frame)
	// Set quit=true if user closed the window
	bool Render(bool &quit);

	// Get the selected scenario path (valid after Render() returns true)
	const std::string &GetSelectedScenario() const { return m_selectedScenario; }

	// Get start-paused state
	bool GetStartPaused() const { return m_startPaused; }

private:
	ScenarioEntry m_root;
	std::string m_selectedScenario;
	std::string m_selectedDescription;
	bool m_startPaused;
	bool m_scenariosLoaded;

	void ScanDirectory(const std::string &dir, const std::string &relPath, ScenarioEntry &parent);
	void RenderTree(ScenarioEntry &entry);
	void LoadDescription(const std::string &fullPath);
};

} // namespace ogl

#endif // !_WIN32
#endif // __OGLLLAUNCHPAD_H
