// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// OGLLaunchpad - ImGui-based pre-simulation launchpad dialog for macOS/Linux
// Provides scenario selection, launch control, and basic options
//
// Tab layout mirrors the Windows Launchpad (Src/Orbiter/Launchpad.cpp):
//   PG_SCN  Scenarios   (TabScenario.cpp)
//   PG_OPT  Options     (TabOptions.cpp)
//   PG_MOD  Modules     (TabModule.cpp)
//   PG_VID  Video       (TabVideo.cpp)
//   PG_EXT  Extra       (TabExtra.cpp)
//   PG_ABT  About       (TabAbout.cpp)

#ifndef __OGLLLAUNCHPAD_H
#define __OGLLLAUNCHPAD_H

#ifndef _WIN32

#include <string>
#include <vector>
#include <functional>

class Config;

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

// Per-tab state structures kept in OGLLaunchpad so they survive across frames.
struct ScenarioTabState {
	float splitterPos = 0.45f;   // left pane fraction of width (0..1)
};

struct OptionsTabState {
	int  flightModelLevel = 0;
	int  damageSetting = 0;
	bool bLimitedFuel = false;
	bool bPadRefuel = true;
	int  mfdSize = 6;
	int  mfdMapVersion = 1;
	bool bMfdTransparent = false;
	bool bGlasspitCompact = false;
	double instrUpdDT = 1.0;
	double panelScale = 1.0;
	double panelScrollSpeed = 200.0;
};

struct VideoTabState {
	int  modeIndex = 0;          // index into modeList
	bool bFullscreen = false;
	bool bNoVsync = false;
	bool bStereo = false;
	bool bTryStencil = true;
	int  winW = 1280, winH = 720;
	std::vector<std::pair<int,int>> modeList; // (w,h) pairs from SDL display modes
};

struct ModuleEntry {
	std::string name;            // logical module name (filename without ext)
	std::string filePath;        // absolute path to .dylib / .so
	std::string description;     // first paragraph of <name>.txt sibling file
	bool active = false;         // currently enabled
};

struct ModulesTabState {
	std::vector<ModuleEntry> modules;
	bool scanned = false;
	int  selected = -1;
};

struct ExtraTabState {
	float splitterPos = 0.40f;
	int   selectedItem = -1;     // index into ExtraTab::Items()
};

struct AboutTabState {
	// purely visual
};

class OGLLaunchpad {
public:
	OGLLaunchpad();
	~OGLLaunchpad();

	// Bind to Orbiter Config — populates initial UI state from cfg, and
	// SetConfig() at launch time writes UI state back.
	void Bind(Config *cfg);

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

	// Push current UI state into bound Config (called automatically on Launch).
	void SyncToConfig();

private:
	Config *m_cfg = nullptr;

	// Cross-tab state
	int  m_currentTab = 0;        // 0..5 → Scenario, Options, Modules, Video, Extra, About
	ScenarioEntry m_root;
	std::string m_selectedScenario;
	std::string m_selectedScenarioFull;
	std::string m_selectedDescription;
	bool m_startPaused = false;
	bool m_scenariosLoaded = false;

	ScenarioTabState m_scn;
	OptionsTabState  m_opt;
	VideoTabState    m_vid;
	ModulesTabState  m_mod;
	ExtraTabState    m_ext;
	AboutTabState    m_abt;

	// Helpers
	void ScanDirectory(const std::string &dir, const std::string &relPath, ScenarioEntry &parent);
	void RenderTree(ScenarioEntry &entry);
	void LoadDescription(const std::string &fullPath);

	// Tab renderers
	void RenderTabScenario(float availH);
	void RenderTabOptions(float availH);
	void RenderTabModules(float availH);
	void RenderTabVideo(float availH);
	void RenderTabExtra(float availH);
	void RenderTabAbout(float availH);

	// Init from Config (called from Bind)
	void InitFromConfig();
};

} // namespace ogl

#endif // !_WIN32
#endif // __OGLLLAUNCHPAD_H
