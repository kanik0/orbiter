// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef _WIN32

#include "OGLMeshRegistry.h"
#include "OGLvVessel.h"  // for CachedMesh destructor (owns GL buffers)

#include <cstdio>

namespace ogl {

MeshRegistry &MeshRegistry::Instance()
{
	static MeshRegistry s_instance;
	return s_instance;
}

MeshRegistry::~MeshRegistry()
{
	Clear();
}

CachedMesh *MeshRegistry::Acquire(MESHHANDLE hMesh)
{
	if (!hMesh) return nullptr;

	std::lock_guard<std::mutex> lock(m_mu);
	auto it = m_entries.find(reinterpret_cast<std::uintptr_t>(hMesh));
	if (it == m_entries.end()) {
		m_misses.fetch_add(1, std::memory_order_relaxed);
		return nullptr;
	}

	Entry &e = it->second;
	if (e.dirty) {
		delete e.mesh;
		e.mesh  = nullptr;
		e.dirty = false;
		m_misses.fetch_add(1, std::memory_order_relaxed);
		return nullptr;
	}
	if (!e.mesh) {
		m_misses.fetch_add(1, std::memory_order_relaxed);
		return nullptr;
	}
	m_hits.fetch_add(1, std::memory_order_relaxed);
	return e.mesh;
}

void MeshRegistry::Store(MESHHANDLE hMesh, CachedMesh *cached)
{
	if (!hMesh) { delete cached; return; }

	std::lock_guard<std::mutex> lock(m_mu);
	Entry &e = m_entries[reinterpret_cast<std::uintptr_t>(hMesh)];
	if (e.mesh && e.mesh != cached)
		delete e.mesh;
	e.mesh  = cached;
	e.dirty = false;
	m_rebuilds.fetch_add(1, std::memory_order_relaxed);
}

void MeshRegistry::InvalidateMesh(MESHHANDLE hMesh)
{
	if (!hMesh) return;
	std::lock_guard<std::mutex> lock(m_mu);
	auto it = m_entries.find(reinterpret_cast<std::uintptr_t>(hMesh));
	if (it == m_entries.end())
		return;
	it->second.dirty = true;
	m_invalidations.fetch_add(1, std::memory_order_relaxed);
}

void MeshRegistry::InvalidateGroup(MESHHANDLE hMesh, DWORD /*grpIdx*/)
{
	// M2 scope: fall back to mesh-wide invalidation. A later milestone can
	// track per-group dirty bits so edits to a single group don't force the
	// whole mesh to be re-uploaded.
	InvalidateMesh(hMesh);
}

void MeshRegistry::Forget(MESHHANDLE hMesh)
{
	if (!hMesh) return;
	std::lock_guard<std::mutex> lock(m_mu);
	auto it = m_entries.find(reinterpret_cast<std::uintptr_t>(hMesh));
	if (it == m_entries.end()) return;
	delete it->second.mesh;
	m_entries.erase(it);
}

void MeshRegistry::Clear()
{
	std::lock_guard<std::mutex> lock(m_mu);
	for (auto &kv : m_entries)
		delete kv.second.mesh;
	m_entries.clear();
}

MeshRegistry::Stats MeshRegistry::Snapshot() const
{
	Stats s{};
	{
		std::lock_guard<std::mutex> lock(m_mu);
		s.entries = m_entries.size();
	}
	s.hits          = m_hits.load(std::memory_order_relaxed);
	s.misses        = m_misses.load(std::memory_order_relaxed);
	s.invalidations = m_invalidations.load(std::memory_order_relaxed);
	s.rebuilds      = m_rebuilds.load(std::memory_order_relaxed);
	return s;
}

void MeshRegistry::LogStatsPeriodic(double minIntervalSec)
{
	using clock = std::chrono::steady_clock;
	const auto now = clock::now();
	if (m_lastLog.time_since_epoch().count() != 0) {
		const double since = std::chrono::duration<double>(now - m_lastLog).count();
		if (since < minIntervalSec)
			return;
	}
	m_lastLog = now;

	Stats s = Snapshot();
	fprintf(stderr,
	        "[MeshReg] entries=%zu hits=%llu misses=%llu rebuilds=%llu invalidations=%llu\n",
	        s.entries,
	        (unsigned long long)s.hits,
	        (unsigned long long)s.misses,
	        (unsigned long long)s.rebuilds,
	        (unsigned long long)s.invalidations);
}

} // namespace ogl

#endif // !_WIN32
