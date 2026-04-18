// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#include "LaunchpadRegistry.h"
#include "OrbiterAPI.h"
#include <cstring>
#include <algorithm>

#ifdef _WIN32
#  include <windows.h>
#  define LR_STRICMP _stricmp
#else
#  include <strings.h>
#  define LR_STRICMP strcasecmp
#endif

namespace orbiter {

LaunchpadRegistry &LaunchpadRegistry::Instance()
{
	static LaunchpadRegistry s;
	return s;
}

LpadHandle LaunchpadRegistry::Register(LaunchpadItem *item, LpadHandle parent)
{
	if (!item) return 0;
	LaunchpadEntry e{ item, m_nextHandle++, parent };
	m_entries.push_back(e);
	return e.handle;
}

bool LaunchpadRegistry::Unregister(LaunchpadItem *item)
{
	auto it = std::find_if(m_entries.begin(), m_entries.end(),
		[item](const LaunchpadEntry &e) { return e.item == item; });
	if (it == m_entries.end()) return false;

	// Reparent any direct children to the unregistered item's parent so
	// tree structure stays sensible.
	LpadHandle removed = it->handle;
	LpadHandle newParent = it->parent;
	m_entries.erase(it);
	for (auto &e : m_entries) {
		if (e.parent == removed) e.parent = newParent;
	}
	return true;
}

LpadHandle LaunchpadRegistry::Find(const char *name, LpadHandle parent) const
{
	if (!name) return 0;
	for (const auto &e : m_entries) {
		if (e.parent != parent) continue;
		const char *n = e.item->Name();
		if (n && LR_STRICMP(n, name) == 0)
			return e.handle;
	}
	return 0;
}

void LaunchpadRegistry::WriteConfigAll()
{
	for (auto &e : m_entries) {
		if (e.item) e.item->clbkWriteConfig();
	}
}

} // namespace orbiter
