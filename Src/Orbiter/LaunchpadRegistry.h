// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// Process-wide registry of LaunchpadItem instances.
//
// On Windows the Launchpad tracks "Extra" items via Win32 tree-view
// HTREEITEM handles owned by the Launchpad dialog (Src/Orbiter/Launchpad.cpp).
// On macOS / Linux the Launchpad is a stateless ImGui form rebuilt every
// frame, so we keep the source-of-truth in this cross-platform registry
// that survives across Launchpad open / close and across module load /
// unload cycles.
//
// Used by:
//   * oapiRegisterLaunchpadItem / oapiUnregisterLaunchpadItem /
//     oapiFindLaunchpadItem (Src/Orbiter/OrbiterAPI.cpp) — non-Windows
//     path forwards into this registry.
//   * OGLLaunchpad::RenderTabExtra (OVP/OGLClient/OGLLaunchpad.cpp) —
//     iterates the registry to build the Extra tab tree.
//   * Built-in items (Physics/Dynamics/Stabilisation/...) on macOS are
//     created and registered here at Orbiter startup so the Extra tab
//     never depends on TabExtra.cpp (Win32-only).

#ifndef __LAUNCHPADREGISTRY_H
#define __LAUNCHPADREGISTRY_H

#include <vector>
#include <cstdint>

class LaunchpadItem;

namespace orbiter {

// Opaque parent handle. We reuse intptr_t so it can round-trip through
// the public LAUNCHPADITEM_HANDLE type used by oapiRegisterLaunchpadItem.
using LpadHandle = std::intptr_t;

struct LaunchpadEntry {
	LaunchpadItem *item;     // owned by caller (we never delete)
	LpadHandle     handle;   // unique non-zero id assigned at register time
	LpadHandle     parent;   // 0 = root, otherwise another entry's handle
};

class LaunchpadRegistry {
public:
	static LaunchpadRegistry &Instance();

	// Register / unregister an item under an optional parent handle.
	// Returns the new entry handle, or 0 on failure.
	LpadHandle Register(LaunchpadItem *item, LpadHandle parent);
	bool       Unregister(LaunchpadItem *item);

	// Look up a child of `parent` whose Name() matches `name` (case
	// insensitive). Returns 0 if not found.
	LpadHandle Find(const char *name, LpadHandle parent) const;

	// Direct read access for the Launchpad UI.
	const std::vector<LaunchpadEntry> &Entries() const { return m_entries; }

	// Invoke clbkWriteConfig() on every registered item — called at
	// Launch time before the simulation session starts, mirroring
	// LaunchpadDialog::WriteExtraParams() on Win32.
	void WriteConfigAll();

private:
	LaunchpadRegistry() = default;
	std::vector<LaunchpadEntry> m_entries;
	LpadHandle m_nextHandle = 1;
};

} // namespace orbiter

#endif // !__LAUNCHPADREGISTRY_H
