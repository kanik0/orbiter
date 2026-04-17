// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// OGLMeshRegistry - Central GPU mesh cache for OGLClient.
//
// Replaces the pre-M2 static map<MESHHANDLE, CachedMesh*> that lived inside
// OGLvVessel with a thread-safe singleton that:
//
//   * exposes explicit Invalidate / Forget entry points so the callback path
//     (oapi::GraphicsClient::clbkEditMeshGroup, clbkSetMeshMaterial, etc.)
//     can mark a cache entry stale and force the render thread to rebuild
//     the VBO/EBO/VAO tuple next time it asks for the mesh;
//   * is safely shared between the module (simulation) thread — which may
//     issue clbkEdit*/clbkSet* calls at any time — and the render thread
//     that consumes the cache;
//   * tracks simple hit/miss/rebuild counters so we can confirm the cache is
//     actually working at runtime (the M2 verifica asks for a log of the
//     form "cache hit: X miss: Y" after ~60 s of sim time).
//
// Only the skeleton of CachedMesh is in here; the actual NTVERTEX → GPU
// upload lives in OGLvVessel.cpp where the vertex attribute layout is
// known. The registry never touches GL state itself.

#ifndef __OGLMESHREGISTRY_H
#define __OGLMESHREGISTRY_H

#ifndef _WIN32
#include <OpenGL/gl3.h>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>

#include "OrbiterAPI.h"

namespace ogl {

struct CachedMesh;  // defined in OGLvVessel.h — owned by the registry once Stored

class MeshRegistry {
public:
	static MeshRegistry &Instance();

	// Look up a cache entry.
	// Returns the cached mesh (hit) or nullptr (miss/dirty). On miss the
	// caller is expected to rebuild the mesh and call Store() with the
	// result. On dirty, the stale CachedMesh is deleted before returning,
	// so the caller's upcoming Store() replaces the slot cleanly.
	CachedMesh *Acquire(MESHHANDLE hMesh);

	// Install a freshly-built cache entry. Takes ownership of `cached`
	// (deletes the previous occupant of the slot, if any).
	void Store(MESHHANDLE hMesh, CachedMesh *cached);

	// Mark a mesh entry dirty — next Acquire() returns nullptr and the
	// stale GPU buffers are freed at that point. Safe to call from the
	// simulation thread.
	void InvalidateMesh(MESHHANDLE hMesh);

	// Group-granular invalidation. For the M2 implementation this is
	// equivalent to InvalidateMesh (full rebuild); a future milestone can
	// drop to per-group rebuild when shape editing dominates the workload.
	void InvalidateGroup(MESHHANDLE hMesh, DWORD grpIdx);

	// Forget a mesh entirely (e.g. vessel destroyed). Frees any GPU buffers
	// held for it.
	void Forget(MESHHANDLE hMesh);

	// Drop every entry. Called from OGLvVessel::ReleaseShared on shutdown.
	void Clear();

	struct Stats {
		std::size_t entries;
		std::uint64_t hits;
		std::uint64_t misses;
		std::uint64_t invalidations;
		std::uint64_t rebuilds;
	};
	Stats Snapshot() const;

	// Emit a one-line [MeshReg] summary if at least `minIntervalSec` have
	// elapsed since the last call that actually logged. Cheap to call every
	// frame — the timing check runs without the registry mutex.
	void LogStatsPeriodic(double minIntervalSec = 5.0);

private:
	MeshRegistry() = default;
	~MeshRegistry();
	MeshRegistry(const MeshRegistry &)            = delete;
	MeshRegistry &operator=(const MeshRegistry &) = delete;

	struct Entry {
		CachedMesh *mesh  = nullptr;
		bool        dirty = false;
	};

	mutable std::mutex         m_mu;
	std::map<std::uintptr_t, Entry> m_entries;

	std::atomic<std::uint64_t> m_hits{0};
	std::atomic<std::uint64_t> m_misses{0};
	std::atomic<std::uint64_t> m_invalidations{0};
	std::atomic<std::uint64_t> m_rebuilds{0};

	std::chrono::steady_clock::time_point m_lastLog{};
};

} // namespace ogl

#endif // !_WIN32
#endif // __OGLMESHREGISTRY_H
