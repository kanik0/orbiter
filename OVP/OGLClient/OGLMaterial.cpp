// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef _WIN32

#include "OGLMaterial.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ogl {

// ---------------------------------------------------------------------------
// BuildMaterialData
// ---------------------------------------------------------------------------

static inline float clampf(float v, float lo, float hi)
{
	return v < lo ? lo : (v > hi ? hi : v);
}

void BuildMaterialData(UBOMaterialData &out, MESHHANDLE hMesh, DWORD grpIdx)
{
	// Sensible defaults used when the group has no material assigned.
	out.diffuse[0]   = out.diffuse[1]   = out.diffuse[2]   = 0.8f; out.diffuse[3] = 1.0f;
	out.specular[0]  = out.specular[1]  = out.specular[2]  = 0.3f; out.specular[3] = 20.0f;
	out.emissive[0]  = out.emissive[1]  = out.emissive[2]  = 0.0f; out.emissive[3] = 0.0f;
	out.reflect[0]   = out.reflect[1]   = out.reflect[2]   = 0.0f; out.reflect[3]  = 0.0f;
	out.roughness    = 0.5f;
	out.metalness    = 0.0f;
	out.opacity      = 1.0f;
	out.fresnelPow   = 5.0f;
	out.hasDiffuse = out.hasNormal = out.hasSpecular = out.hasEmissive = 0;
	out.hasRoughness = out.hasMetalness = out.hasEnvMap = 0;
	out.pad0 = 0;

	if (!hMesh) return;

	MESHGROUPEX *grp = oapiMeshGroupEx(hMesh, grpIdx);
	if (!grp) return;
	if (grp->MtrlIdx == 0) return;  // group uses no material

	DWORD nMat = oapiMeshMaterialCount(hMesh);
	if (grp->MtrlIdx > nMat) return;

	MATERIAL *mat = oapiMeshMaterial(hMesh, grp->MtrlIdx - 1);  // MtrlIdx is 1-based
	if (!mat) return;

	out.diffuse[0]  = mat->diffuse.r;
	out.diffuse[1]  = mat->diffuse.g;
	out.diffuse[2]  = mat->diffuse.b;
	out.diffuse[3]  = mat->diffuse.a;
	out.specular[0] = mat->specular.r;
	out.specular[1] = mat->specular.g;
	out.specular[2] = mat->specular.b;
	out.specular[3] = mat->power;
	out.emissive[0] = mat->emissive.r;
	out.emissive[1] = mat->emissive.g;
	out.emissive[2] = mat->emissive.b;
	out.opacity     = mat->diffuse.a;

	// Convert legacy Blinn-Phong power → PBR roughness using the mapping the
	// D3D9Client MaterialMgr uses (log2(power) / 12, clamped). Very smooth
	// (power >= 4096) → roughness 0; no power → 0.5.
	if (mat->power > 1.0f)
		out.roughness = clampf(1.0f - float(std::log2(mat->power)) / 12.0f, 0.04f, 1.0f);
}

// ---------------------------------------------------------------------------
// MaterialStore
// ---------------------------------------------------------------------------

MaterialStore &MaterialStore::Instance()
{
	static MaterialStore s_instance;
	return s_instance;
}

int MaterialStore::Set(DEVMESHHANDLE hMesh, DWORD matidx, MatProp prp,
                       const oapi::FVECTOR4 &value)
{
	if (!hMesh) return 2;
	switch (prp) {
	case MatProp::Diffuse:
	case MatProp::Ambient:
	case MatProp::Specular:
	case MatProp::Light:
	case MatProp::Emission:
	case MatProp::Reflect:
	case MatProp::Smooth:
	case MatProp::Metal:
	case MatProp::Fresnel:
	case MatProp::SpecialFX:
		break;
	default:
		return 2;
	}

	Key k{ reinterpret_cast<std::uintptr_t>(hMesh),
	        (std::uint32_t)matidx,
	        (std::uint32_t)prp };
	std::lock_guard<std::mutex> lock(m_mu);
	m_map[k] = value;
	return 0;
}

int MaterialStore::Get(DEVMESHHANDLE hMesh, DWORD matidx, MatProp prp,
                       oapi::FVECTOR4 &out) const
{
	if (!hMesh) return 1;
	Key k{ reinterpret_cast<std::uintptr_t>(hMesh),
	        (std::uint32_t)matidx,
	        (std::uint32_t)prp };
	std::lock_guard<std::mutex> lock(m_mu);
	auto it = m_map.find(k);
	if (it == m_map.end()) return 1;
	out = it->second;
	return 0;
}

void MaterialStore::Apply(UBOMaterialData &data, DEVMESHHANDLE hMesh, DWORD matidx) const
{
	if (!hMesh) return;
	std::lock_guard<std::mutex> lock(m_mu);

	auto lookup = [&](MatProp prp, const oapi::FVECTOR4 **out) -> bool {
		Key k{ reinterpret_cast<std::uintptr_t>(hMesh),
		        (std::uint32_t)matidx,
		        (std::uint32_t)prp };
		auto it = m_map.find(k);
		if (it == m_map.end()) return false;
		*out = &it->second;
		return true;
	};

	const oapi::FVECTOR4 *v = nullptr;
	if (lookup(MatProp::Diffuse, &v)) {
		data.diffuse[0] = v->r; data.diffuse[1] = v->g;
		data.diffuse[2] = v->b; data.diffuse[3] = v->a;
		data.opacity    = v->a;
	}
	if (lookup(MatProp::Specular, &v)) {
		data.specular[0] = v->r; data.specular[1] = v->g;
		data.specular[2] = v->b; data.specular[3] = v->a;
	}
	if (lookup(MatProp::Emission, &v) || lookup(MatProp::Light, &v)) {
		data.emissive[0] = v->r; data.emissive[1] = v->g;
		data.emissive[2] = v->b; data.emissive[3] = v->a;
	}
	if (lookup(MatProp::Reflect, &v)) {
		data.reflect[0] = v->r; data.reflect[1] = v->g;
		data.reflect[2] = v->b; data.reflect[3] = v->a;
	}
	if (lookup(MatProp::Smooth, &v)) {
		// smoothness 1.0 → roughness 0.0
		data.roughness = clampf(1.0f - v->r, 0.04f, 1.0f);
	}
	if (lookup(MatProp::Metal, &v)) {
		data.metalness = clampf(v->r, 0.0f, 1.0f);
	}
	if (lookup(MatProp::Fresnel, &v)) {
		data.fresnelPow = v->r;
	}
	// MatProp::SpecialFX is reserved for heat-map FX — routed through a
	// dedicated shader path in a later milestone; we intentionally do not
	// fold it into the standard material block.
}

void MaterialStore::Forget(DEVMESHHANDLE hMesh)
{
	if (!hMesh) return;
	std::lock_guard<std::mutex> lock(m_mu);
	const std::uintptr_t meshPtr = reinterpret_cast<std::uintptr_t>(hMesh);
	for (auto it = m_map.begin(); it != m_map.end(); ) {
		if (it->first.mesh == meshPtr)
			it = m_map.erase(it);
		else
			++it;
	}
}

void MaterialStore::Clear()
{
	std::lock_guard<std::mutex> lock(m_mu);
	m_map.clear();
}

} // namespace ogl

#endif // !_WIN32
