// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// Cross-platform Builtin LaunchpadItem registration.
//
// On Windows the Launchpad's Extra tab populates itself in
// ExtraTab::Create() (Src/Orbiter/TabExtra.cpp) by instantiating
// 15 BuiltinLaunchpadItem subclasses (ExtraPropagation,
// ExtraDynamics, ExtraStabilisation, ExtraInstruments,
// ExtraMfdConfig, ExtraVesselConfig, ExtraPlanetConfig,
// ExtraDebug, ExtraShutdown, ExtraFixedStep,
// ExtraRenderingOptions, ExtraTimerSettings,
// ExtraPerformanceSettings, ExtraLaunchpadOptions,
// ExtraLogfileOptions). Each of those classes uses HWND-based
// Win32 dialogs.
//
// On macOS / Linux we provide a parallel set of items implemented
// via LaunchpadItem::clbkRender() (ImGui), so the Extra tab in
// OGLLaunchpad shows the same hierarchy and editors with no
// degraded-functionality items. The registration is done once at
// Orbiter startup.

#ifndef __BUILTINLAUNCHPADITEMS_H
#define __BUILTINLAUNCHPADITEMS_H

class Config;

namespace orbiter {

#ifndef _WIN32
// Instantiate the 15 builtin Extra items and add them to the global
// LaunchpadRegistry. Items remain alive for the lifetime of the
// process (kept in a function-local static vector).
void RegisterBuiltinLaunchpadItems(Config *cfg);
#endif

} // namespace orbiter

#endif // !__BUILTINLAUNCHPADITEMS_H
