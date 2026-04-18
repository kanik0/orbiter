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

// Lazy-loaded RGBA thumbnail uploaded as a GL texture and shown via ImGui::Image.
struct ScenarioThumbnail {
	unsigned int texId = 0;  // GLuint, 0 = not loaded / no thumbnail available
	int width = 0, height = 0;
	bool tried = false;      // already attempted load (prevents retry on miss)
};

// Per-tab state structures kept in OGLLaunchpad so they survive across frames.
struct ScenarioTabState {
	float splitterPos = 0.45f;   // left pane fraction of width (0..1)
};

// Options tab UI state. Page editors read and write Config fields
// directly through m_cfg; only navigation lives here.
struct OptionsTabState {
	int currentPage = 0;          // 0..11 → Visual / Physics / Instrument /
	                              // Vessel / UI / Joystick / CelSphere /
	                              // VisHelper / Planetarium / Labels /
	                              // Forces / Axes
	float splitterPos = 0.30f;    // left navigation list width fraction
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
	std::string filePath;        // absolute path to .dylib / .so / .dll
	std::string category;        // category from .info or "Miscellaneous"
	std::string description;     // free-form text from .info, may be empty
	bool active = false;         // currently enabled
	bool locked = false;         // forced active by command-line --plugin
};

struct ModulesTabState {
	std::vector<ModuleEntry> modules;
	bool scanned = false;
	int  selected = -1;          // index into modules, -1 = none
	std::string pluginDir;       // absolute path to Modules/Plugin
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

	// Scan Modules/Plugin/ for plugin .dylib / .so / .dll files and the
	// matching <name>.info metadata sidecars. Existing active state is
	// seeded from Config::IsActiveModule and CfgCmdlinePrm.LoadPlugins.
	void ScanModules(const std::string &pluginDir);

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
	bool m_selectionIsFolder = false;
	ScenarioThumbnail m_selectedThumb;
	bool m_startPaused = false;
	bool m_scenariosLoaded = false;
	bool m_draggingScnSplitter = false;

	ScenarioTabState m_scn;
	OptionsTabState  m_opt;
	VideoTabState    m_vid;
	ModulesTabState  m_mod;
	ExtraTabState    m_ext;
	AboutTabState    m_abt;

	// Helpers
	void ScanDirectory(const std::string &dir, const std::string &relPath, ScenarioEntry &parent);
	void RenderTree(ScenarioEntry &entry);
	// Load description text for the currently selected entry. For a scenario file
	// the BEGIN_DESC block of the .scn file is parsed; for a folder, the sibling
	// Description.txt is parsed (DESC, then HYPERDESC fallback with HTML stripped).
	void LoadDescription(const std::string &fullPath, bool isFolder);
	// Try to load <scenario>.{jpg,png,bmp} sibling, upload to a GL texture and
	// store the handle in m_selectedThumb. Releases any previously held thumb.
	void LoadThumbnail(const std::string &scenarioFullPath);
	void ReleaseThumbnail();
	// Decode HYPERDESC HTML markup into plain text (mirrors Win32 Html2Text).
	static std::string HtmlToPlainText(const std::string &html);

	// Parse a sidecar .info file (see cmake/orbiter_module_info.cmake) into
	// the (category, description) fields of `entry`. Missing file or
	// parse failure leaves the entry's defaults intact.
	static void ParseModuleInfo(const std::string &infoPath, ModuleEntry &entry);

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
