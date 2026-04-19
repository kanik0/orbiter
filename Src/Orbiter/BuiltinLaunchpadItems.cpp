// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef _WIN32

#include "BuiltinLaunchpadItems.h"
#include "Config.h"
#include "OrbiterAPI.h"
#include "LaunchpadRegistry.h"
#include "imgui.h"
#include <cstring>
#include <vector>
#include <memory>
#include <cstdio>

// Constants for propagator selection / step counts. Mirrors the values
// used by the Win32 ExtraDynamics dialog (TabExtra.cpp:355).
#ifndef NPROP_METHOD
#  include "Config.h"
#endif

namespace {

// Cast helpers — char* return values are required by the public API.
inline char *cstr(const char *s) { return const_cast<char*>(s); }

// ---- Container items (Name + Description, no editor) ----------------

class ContainerItem : public LaunchpadItem {
public:
	ContainerItem(const char *name, const char *desc)
		: m_name(name), m_desc(desc) {}
	char *Name() override        { return cstr(m_name); }
	char *Description() override { return cstr(m_desc); }
	bool  clbkRender() override  { return false; } // no editor
private:
	const char *m_name;
	const char *m_desc;
};

// ---- Editable items (small ImGui forms) -----------------------------
// All editors take a Config* pointer and write directly into the
// matching CFG_* substructure. Returning `true` from clbkRender keeps
// the modal open; the host modal also closes when the user clicks
// the standard "Close" button rendered around clbkRender().

class ItemDynamics : public LaunchpadItem {
public:
	ItemDynamics(Config *cfg) : m_cfg(cfg) {}
	char *Name() override { return cstr("Dynamic state propagation"); }
	char *Description() override {
		return cstr("Multi-stage time-step controller for vessel state "
			"propagation. Each stage selects an integrator order plus a "
			"target time / angle step. Higher stages activate at higher "
			"time accelerations.");
	}
	bool clbkRender() override {
		auto &p = m_cfg->CfgPhysicsPrm;
		ImGui::SliderInt("Active stages", &p.nLPropLevel, 1, MAX_PROP_LEVEL);
		const char *meth[NPROP_METHOD] = {
			"RK2","RK4","RK5","RK6","RK7","RK8","SY2","SY4","SY6","SY8"
		};
		for (int i = 0; i < p.nLPropLevel; ++i) {
			ImGui::PushID(i);
			ImGui::Text("Stage %d", i+1);
			ImGui::SameLine();
			ImGui::PushItemWidth(80.0f);
			ImGui::Combo("##m", &p.PropMode[i], meth, NPROP_METHOD);
			ImGui::PopItemWidth();
			ImGui::PushItemWidth(110.0f);
			ImGui::SameLine();
			ImGui::InputDouble("Δt##t", &p.PropTTgt[i], 0.0, 0.0, "%.3g");
			ImGui::SameLine();
			ImGui::InputDouble("Δa##a", &p.PropATgt[i], 0.0, 0.0, "%.3g");
			ImGui::PopItemWidth();
			if (i < p.nLPropLevel - 1) {
				ImGui::PushItemWidth(110.0f);
				ImGui::SameLine();
				ImGui::InputDouble("max Δt##l", &p.PropTLim[i], 0.0, 0.0, "%.3g");
				ImGui::SameLine();
				ImGui::InputDouble("max Δa##L", &p.PropALim[i], 0.0, 0.0, "%.3g");
				ImGui::PopItemWidth();
			}
			ImGui::PopID();
		}
		ImGui::SliderInt("Max sub-samples", &p.PropSubMax, 1, 200);
		return true;
	}
private:
	Config *m_cfg;
};

class ItemStabilisation : public LaunchpadItem {
public:
	ItemStabilisation(Config *cfg) : m_cfg(cfg) {}
	char *Name() override { return cstr("Orbit stabilisation"); }
	char *Description() override {
		return cstr("Encke's method stabilises Keplerian orbits at high "
			"time acceleration by integrating only the perturbation. "
			"The thresholds below decide when stabilisation kicks in.");
	}
	bool clbkRender() override {
		auto &p = m_cfg->CfgPhysicsPrm;
		ImGui::Checkbox("Enable Encke stabilisation", &p.bOrbitStabilise);
		ImGui::InputDouble("Perturbation limit", &p.Stabilise_PLimit, 0.0, 0.0, "%.4g");
		ImGui::InputDouble("Step-size limit",    &p.Stabilise_SLimit, 0.0, 0.0, "%.4g");
		ImGui::InputDouble("Sub-step target",    &p.PPropSubLimit,   0.0, 0.0, "%.4g");
		ImGui::InputInt   ("Max sub-steps",      &p.PPropSubMax);
		ImGui::InputDouble("Step limit",         &p.PPropStepLimit,  0.0, 0.0, "%.4g");
		return true;
	}
private:
	Config *m_cfg;
};

class ItemMfdConfig : public LaunchpadItem {
public:
	ItemMfdConfig(Config *cfg) : m_cfg(cfg) {}
	char *Name() override { return cstr("MFD parameters"); }
	char *Description() override {
		return cstr("Texture sizing and pow2 policy for multi-functional "
			"display surfaces in 2D panels and virtual cockpits.");
	}
	bool clbkRender() override {
		auto &p = m_cfg->CfgInstrumentPrm;
		const char *pow2[] = { "No", "Yes", "Auto" };
		if (p.bMfdPow2 < 0 || p.bMfdPow2 > 2) p.bMfdPow2 = 2;
		ImGui::Combo("Force pow2 textures", &p.bMfdPow2, pow2, IM_ARRAYSIZE(pow2));
		ImGui::SliderInt("Pow2 hi-res threshold", &p.MfdHiresThreshold, 64, 1024);

		const char *sizes[] = { "256", "512", "1024", "2048" };
		auto idx = [](int v){ switch(v){case 256:return 0;case 512:return 1;
			case 1024:return 2;case 2048:return 3;} return 1; };
		auto val = [](int i){ const int s[] = {256,512,1024,2048}; return s[i]; };
		int p2d = idx(p.PanelMFDHUDSize);
		if (ImGui::Combo("2D panel MFD/HUD", &p2d, sizes, IM_ARRAYSIZE(sizes)))
			p.PanelMFDHUDSize = val(p2d);
		int pvc = idx(p.VCMFDSize);
		if (ImGui::Combo("Virtual cockpit MFD", &pvc, sizes, IM_ARRAYSIZE(sizes)))
			p.VCMFDSize = val(pvc);
		return true;
	}
private:
	Config *m_cfg;
};

class ItemShutdown : public LaunchpadItem {
public:
	ItemShutdown(Config *cfg) : m_cfg(cfg) {}
	char *Name() override { return cstr("Session shutdown"); }
	char *Description() override {
		return cstr("How Orbiter behaves when the simulation session "
			"ends - return to the Launchpad, immediately respawn, or "
			"terminate the process.");
	}
	bool clbkRender() override {
		auto &p = m_cfg->CfgDebugPrm;
		const char *modes[] = {
			"Re-display Launchpad",
			"Respawn (skip Launchpad)",
			"Terminate Orbiter"
		};
		if (p.ShutdownMode < 0 || p.ShutdownMode >= IM_ARRAYSIZE(modes))
			p.ShutdownMode = 0;
		ImGui::Combo("Shutdown mode", &p.ShutdownMode, modes, IM_ARRAYSIZE(modes));
		ImGui::Checkbox("Save exit screenshot", &p.bSaveExitScreen);
		return true;
	}
private:
	Config *m_cfg;
};

class ItemFixedStep : public LaunchpadItem {
public:
	ItemFixedStep(Config *cfg) : m_cfg(cfg) {}
	char *Name() override { return cstr("Fixed time steps"); }
	char *Description() override {
		return cstr("Force Orbiter to advance the simulation by a fixed "
			"time interval each frame instead of the wall-clock interval. "
			"Useful for reproducible playback and frame-locked recording.");
	}
	bool clbkRender() override {
		auto &p = m_cfg->CfgDebugPrm;
		bool enabled = p.FixedStep > 0;
		if (ImGui::Checkbox("Enable fixed step", &enabled))
			p.FixedStep = enabled ? 0.02 : 0.0;
		if (enabled)
			ImGui::InputDouble("Step length [s]", &p.FixedStep, 0.0, 0.0, "%.4g");
		return true;
	}
private:
	Config *m_cfg;
};

class ItemRenderingOptions : public LaunchpadItem {
public:
	ItemRenderingOptions(Config *cfg) : m_cfg(cfg) {}
	char *Name() override { return cstr("Rendering options"); }
	char *Description() override {
		return cstr("Renderer debug toggles - wireframe display, "
			"force-normalisation of mesh normals.");
	}
	bool clbkRender() override {
		auto &p = m_cfg->CfgDebugPrm;
		ImGui::Checkbox("Wireframe mode",       &p.bWireframeMode);
		ImGui::Checkbox("Normalise normals",    &p.bNormaliseNormals);
		return true;
	}
private:
	Config *m_cfg;
};

class ItemTimerSettings : public LaunchpadItem {
public:
	ItemTimerSettings(Config *cfg) : m_cfg(cfg) {}
	char *Name() override { return cstr("Timer settings"); }
	char *Description() override {
		return cstr("Choose between the high-resolution hardware timer "
			"and the lower-resolution software timer for scheduling "
			"frame updates.");
	}
	bool clbkRender() override {
		auto &p = m_cfg->CfgDebugPrm;
		const char *modes[] = { "Auto",
			"Hi-res hardware",
			"Lo-res software" };
		if (p.TimerMode < 0 || p.TimerMode >= IM_ARRAYSIZE(modes))
			p.TimerMode = 0;
		ImGui::Combo("Timer mode", &p.TimerMode, modes, IM_ARRAYSIZE(modes));
		return true;
	}
private:
	Config *m_cfg;
};

class ItemPerformanceSettings : public LaunchpadItem {
public:
	ItemPerformanceSettings(Config *cfg) : m_cfg(cfg) {}
	char *Name() override { return cstr("Performance settings"); }
	char *Description() override {
		return cstr("Performance-related debug toggles. Disable font "
			"smoothing for additional headroom on slow hosts.");
	}
	bool clbkRender() override {
		auto &p = m_cfg->CfgDebugPrm;
		ImGui::Checkbox("Disable font smoothing",       &p.bDisableSmoothFont);
		ImGui::Checkbox("Force re-enable on exit",      &p.bForceReenableSmoothFont);
		return true;
	}
private:
	Config *m_cfg;
};

class ItemLaunchpadOptions : public LaunchpadItem {
public:
	ItemLaunchpadOptions(Config *cfg) : m_cfg(cfg) {}
	char *Name() override { return cstr("Launchpad display options"); }
	char *Description() override {
		return cstr("How the Launchpad renders scenario descriptions: "
			"as plain text or via the inline HTML viewer (Win32 only - "
			"macOS always renders plain text).");
	}
	bool clbkRender() override {
		auto &p = m_cfg->CfgDebugPrm;
		const char *modes[] = { "Plain text",
			"Inline HTML viewer (Win32)",
			"Auto-detect" };
		if (p.bHtmlScnDesc < 0 || p.bHtmlScnDesc >= IM_ARRAYSIZE(modes))
			p.bHtmlScnDesc = 2;
		ImGui::Combo("Description rendering", &p.bHtmlScnDesc, modes, IM_ARRAYSIZE(modes));
		ImGui::TextDisabled("On macOS the inline HTML viewer is unavailable; "
			"the Launchpad falls back to plain text regardless of this setting.");
		return true;
	}
private:
	Config *m_cfg;
};

class ItemLogfileOptions : public LaunchpadItem {
public:
	ItemLogfileOptions(Config *cfg) : m_cfg(cfg) {}
	char *Name() override { return cstr("Log file options"); }
	char *Description() override {
		return cstr("Verbosity of Orbiter.log. Verbose mode logs every "
			"vessel update, useful for diagnosing addon problems.");
	}
	bool clbkRender() override {
		auto &p = m_cfg->CfgDebugPrm;
		ImGui::Checkbox("Verbose log", &p.bVerboseLog);
		return true;
	}
private:
	Config *m_cfg;
};

} // namespace

namespace orbiter {

void RegisterBuiltinLaunchpadItems(Config *cfg)
{
	static std::vector<std::unique_ptr<LaunchpadItem>> s_items;
	if (!s_items.empty()) return; // idempotent

	auto &reg = LaunchpadRegistry::Instance();

	auto add = [&](LaunchpadItem *it, LpadHandle parent) {
		s_items.emplace_back(it);
		return reg.Register(it, parent);
	};

	// Containers — top-level groupings
	LpadHandle hPhys    = add(new ContainerItem("Physics engine",
		"Settings for the dynamic state propagator and orbit "
		"stabilisation."), 0);
	LpadHandle hInst    = add(new ContainerItem("Instruments and panels",
		"Configuration of MFD instruments, panels and virtual cockpit "
		"display surfaces."), 0);
	add(new ContainerItem("Vessel configuration",
		"Container for plugin-registered per-vessel configuration items."), 0);
	add(new ContainerItem("Planet configuration",
		"Container for plugin-registered per-planet configuration items."), 0);
	LpadHandle hDebug   = add(new ContainerItem("Debug options",
		"Diagnostic and developer-oriented settings."), 0);

	// Physics children
	add(new ItemDynamics(cfg),       hPhys);
	add(new ItemStabilisation(cfg),  hPhys);

	// Instruments children
	add(new ItemMfdConfig(cfg),      hInst);

	// Debug children
	add(new ItemShutdown(cfg),                hDebug);
	add(new ItemFixedStep(cfg),               hDebug);
	add(new ItemRenderingOptions(cfg),        hDebug);
	add(new ItemTimerSettings(cfg),           hDebug);
	add(new ItemPerformanceSettings(cfg),     hDebug);
	add(new ItemLaunchpadOptions(cfg),        hDebug);
	add(new ItemLogfileOptions(cfg),          hDebug);

	fprintf(stderr, "[OGLLaunchpad] Registered %zu built-in Extra items\n",
		s_items.size());
}

} // namespace orbiter

#endif // !_WIN32
