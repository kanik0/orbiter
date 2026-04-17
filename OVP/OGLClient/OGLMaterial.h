// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// OGLMaterial - CPU-side mirror of the `Material` UBO plus the override store
// fed by clbkSetMeshMaterialEx.
//
// The `UBOMaterialData` layout matches `shaders/include/material.glsl.inc`
// std140 layout byte-for-byte (112 bytes total). Any change to one side must
// be reflected in the other.
//
// `MaterialStore` is a thread-safe singleton that holds per-(mesh, matidx)
// extended material overrides keyed by oapi::MatProp; `Apply()` patches a
// freshly-built `UBOMaterialData` with the user overrides just before it is
// uploaded to the GPU.

#ifndef __OGLMATERIAL_H
#define __OGLMATERIAL_H

#ifndef _WIN32
#include <cstdint>
#include <mutex>
#include <unordered_map>

#include "OrbiterAPI.h"
#include "DrawAPI.h"  // oapi::FVECTOR4

namespace ogl {

// 112-byte std140 block — must match shaders/include/material.glsl.inc.
struct alignas(16) UBOMaterialData {
	float diffuse[4];       // offset  0 — vec4 rgba
	float specular[4];      // offset 16 — vec4 rgb + power
	float emissive[4];      // offset 32 — vec4 rgb + pad
	float reflect[4];       // offset 48 — vec4 rgb + pad (F0 override)
	float roughness;        // offset 64
	float metalness;        // offset 68
	float opacity;          // offset 72
	float fresnelPow;       // offset 76
	int32_t hasDiffuse;     // offset 80
	int32_t hasNormal;      // offset 84
	int32_t hasSpecular;    // offset 88
	int32_t hasEmissive;    // offset 92
	int32_t hasRoughness;   // offset 96
	int32_t hasMetalness;   // offset 100
	int32_t hasEnvMap;      // offset 104
	int32_t pad0;           // offset 108
};
static_assert(sizeof(UBOMaterialData) == 112,
              "UBOMaterialData size must match std140 Material block (112 bytes)");

// Populate `out` from the Orbiter MESH material referenced by `grp->MtrlIdx`.
// Defaults are used when the mesh has no explicit material. The legacy
// spec-power → roughness approximation (log2(power)/12) is applied so legacy
// meshes still look correct under the PBR shader.
//
// After BuildMaterialData, callers should invoke MaterialStore::Apply() to
// overlay any runtime overrides set through clbkSetMeshMaterialEx.
void BuildMaterialData(UBOMaterialData &out, MESHHANDLE hMesh, DWORD grpIdx);

// Thread-safe store for MatProp overrides installed by vessel modules.
class MaterialStore {
public:
	static MaterialStore &Instance();

	// Install / replace an override. Returns 0 on success, 2 when the
	// property isn't recognised (mirrors clbkSetMeshMaterialEx contract).
	int Set(DEVMESHHANDLE hMesh, DWORD matidx, MatProp prp,
	        const oapi::FVECTOR4 &value);

	// Read back an override. Returns 0 if a value was written to `out`,
	// 1 when no override is registered for that slot.
	int Get(DEVMESHHANDLE hMesh, DWORD matidx, MatProp prp,
	        oapi::FVECTOR4 &out) const;

	// Apply every override registered for (hMesh, matidx) onto `data`.
	void Apply(UBOMaterialData &data, DEVMESHHANDLE hMesh, DWORD matidx) const;

	// Drop all overrides for a given mesh (called from Forget hooks in
	// future milestones — currently unused but kept symmetric with the
	// MeshRegistry API).
	void Forget(DEVMESHHANDLE hMesh);

	// Drop every override on shutdown.
	void Clear();

private:
	MaterialStore() = default;
	~MaterialStore() = default;
	MaterialStore(const MaterialStore &)            = delete;
	MaterialStore &operator=(const MaterialStore &) = delete;

	struct Key {
		std::uintptr_t mesh;
		std::uint32_t  matidx;
		std::uint32_t  prp;
		bool operator==(const Key &o) const {
			return mesh == o.mesh && matidx == o.matidx && prp == o.prp;
		}
	};
	struct KeyHash {
		std::size_t operator()(const Key &k) const noexcept {
			// mix mesh pointer + matidx + prp
			std::size_t h = k.mesh;
			h ^= std::size_t(k.matidx) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
			h ^= std::size_t(k.prp)    + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
			return h;
		}
	};

	mutable std::mutex m_mu;
	std::unordered_map<Key, oapi::FVECTOR4, KeyHash> m_map;
};

} // namespace ogl

#endif // !_WIN32
#endif // __OGLMATERIAL_H
