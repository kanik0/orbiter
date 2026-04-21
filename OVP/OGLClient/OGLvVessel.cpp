// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef _WIN32

#include "OGLvVessel.h"
#include "OGLShaderMgr.h"
#include "OGLMaterial.h"
#include "OGLEnvMap.h"
#include "OGLShadowMap.h"
#include "OGLMeshRegistry.h"
#include "OGLTexture.h"
#include "OGLSurface.h"
#include "VesselAPI.h"
#include "GraphicsAPI.h"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <array>
#include <cstdint>
#include <unordered_map>
#include <sys/stat.h>

namespace ogl {

// Static members
GLuint OGLvVessel::s_vesselShader = 0;
GLuint OGLvVessel::s_pbrShader = 0;
GLuint OGLvVessel::s_exhaustShader = 0;
GLuint OGLvVessel::s_exhaustVAO = 0, OGLvVessel::s_exhaustVBO = 0, OGLvVessel::s_exhaustEBO = 0;
GLuint OGLvVessel::s_materialUBO = 0;
GLuint OGLvVessel::s_shadowShader = 0;
OGLShadowMap *OGLvVessel::s_shadowMap = nullptr;
OGLTexture *OGLvVessel::s_exhaustTexture = nullptr;
bool OGLvVessel::s_sharedInitialized = false;
ShaderMgr *OGLvVessel::s_shaderMgr = nullptr;
OGLEnvMap *OGLvVessel::s_envMap = nullptr;
oapi::GraphicsClient *OGLvVessel::s_gc = nullptr;
std::map<std::string, MESHHANDLE> OGLvVessel::s_fallbackMeshes;

static bool FileExists(const char *path) {
	struct stat st;
	return stat(path, &st) == 0;
}

// ----- Mesh-group animation transforms (issue #94) --------------------------
// Orbiter vessels attach MGROUP_TRANSFORM animations to mesh groups via
// Vessel::AddAnimationComponent. The transforms move individual groups
// (struts, hatches, control surfaces, etc.) relative to the authored mesh
// when the vessel's animation state changes — for example the DeltaGlider
// rotates each landing-gear group by up to ~95° as anim_gear travels from
// defstate to 0.  We evaluate the transforms per-frame and compose them
// into each group's model matrix instead of mutating the shared mesh VBO,
// since the mesh cache is shared across vessels that use the same template.
namespace {

using Mat4 = std::array<float, 16>;

constexpr Mat4 kIdentity4 = {
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, 1
};

// Column-major 4x4 multiply — c = a * b, indexing m[col*4 + row].
inline Mat4 MatMul(const Mat4 &a, const Mat4 &b) {
    Mat4 c{};
    for (int j = 0; j < 4; ++j)
        for (int i = 0; i < 4; ++i) {
            float v = 0.0f;
            for (int k = 0; k < 4; ++k) v += a[k * 4 + i] * b[j * 4 + k];
            c[j * 4 + i] = v;
        }
    return c;
}

// Rotation by `angle` rad around `axis`, pivoting around `ref`.
// Equivalent to T(ref) * R(axis, angle) * T(-ref).
inline Mat4 RotAxisRef(const VECTOR3 &axis, double angle, const VECTOR3 &ref) {
    double ax = axis.x, ay = axis.y, az = axis.z;
    const double len = std::sqrt(ax * ax + ay * ay + az * az);
    if (len < 1e-12) return kIdentity4;
    ax /= len; ay /= len; az /= len;
    const double c = std::cos(angle), s = std::sin(angle), omc = 1.0 - c;
    const float r00 = (float)(c + ax * ax * omc);
    const float r01 = (float)(ax * ay * omc - az * s);
    const float r02 = (float)(ax * az * omc + ay * s);
    const float r10 = (float)(ay * ax * omc + az * s);
    const float r11 = (float)(c + ay * ay * omc);
    const float r12 = (float)(ay * az * omc - ax * s);
    const float r20 = (float)(az * ax * omc - ay * s);
    const float r21 = (float)(az * ay * omc + ax * s);
    const float r22 = (float)(c + az * az * omc);
    const float rx = (float)ref.x, ry = (float)ref.y, rz = (float)ref.z;
    const float tx = rx - (r00 * rx + r01 * ry + r02 * rz);
    const float ty = ry - (r10 * rx + r11 * ry + r12 * rz);
    const float tz = rz - (r20 * rx + r21 * ry + r22 * rz);
    return {
        r00, r10, r20, 0.0f,
        r01, r11, r21, 0.0f,
        r02, r12, r22, 0.0f,
        tx,  ty,  tz,  1.0f
    };
}

inline Mat4 Translate(double x, double y, double z) {
    return {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        (float)x, (float)y, (float)z, 1
    };
}

inline Mat4 ScaleRef(const VECTOR3 &scale, const VECTOR3 &ref) {
    const float sx = (float)scale.x, sy = (float)scale.y, sz = (float)scale.z;
    const float rx = (float)ref.x,   ry = (float)ref.y,   rz = (float)ref.z;
    return {
        sx, 0,  0,  0,
        0,  sy, 0,  0,
        0,  0,  sz, 0,
        rx * (1.0f - sx), ry * (1.0f - sy), rz * (1.0f - sz), 1
    };
}

using AnimXformMap = std::unordered_map<uint64_t, Mat4>;

inline uint64_t AnimKey(UINT mesh, UINT grp) {
    return ((uint64_t)mesh << 32) | (uint64_t)grp;
}

static void ProcessAnimComp(const ANIMATIONCOMP *comp, double state, double defstate,
                            const Mat4 &parentT, AnimXformMap &out)
{
    if (!comp || !comp->trans) return;
    const MGROUP_TRANSFORM *trans = comp->trans;
    const double s0 = comp->state0, s1 = comp->state1;
    const double sEff = std::clamp(state,    s0, s1);
    const double dEff = std::clamp(defstate, s0, s1);
    const double range = s1 - s0;
    const double progress = (range > 1e-12) ? (sEff - dEff) / range : 0.0;

    Mat4 local = kIdentity4;
    switch (trans->Type()) {
    case MGROUP_TRANSFORM::NULLTRANSFORM:
        break;
    case MGROUP_TRANSFORM::ROTATE: {
        const MGROUP_ROTATE *r = static_cast<const MGROUP_ROTATE *>(trans);
        local = RotAxisRef(r->axis, r->angle * progress, r->ref);
    } break;
    case MGROUP_TRANSFORM::TRANSLATE: {
        const MGROUP_TRANSLATE *l = static_cast<const MGROUP_TRANSLATE *>(trans);
        local = Translate(l->shift.x * progress, l->shift.y * progress, l->shift.z * progress);
    } break;
    case MGROUP_TRANSFORM::SCALE: {
        const MGROUP_SCALE *s = static_cast<const MGROUP_SCALE *>(trans);
        // Interpolate each axis between 1.0 (authored) and scale as progress 0→1.
        const VECTOR3 sc = {
            1.0 + (s->scale.x - 1.0) * progress,
            1.0 + (s->scale.y - 1.0) * progress,
            1.0 + (s->scale.z - 1.0) * progress
        };
        local = ScaleRef(sc, s->ref);
    } break;
    }

    const Mat4 composed = MatMul(parentT, local);

    // Skip per-vertex lists (LOCALVERTEXLIST) — a group-level matrix can't
    // express arbitrary per-vertex edits. Vessels that use that path fall
    // back to the authored mesh, matching the pre-animation behaviour.
    if (trans->mesh != LOCALVERTEXLIST && trans->grp) {
        for (UINT i = 0; i < trans->ngrp; ++i)
            out[AnimKey(trans->mesh, trans->grp[i])] = composed;
    }

    for (UINT i = 0; i < comp->nchildren; ++i)
        ProcessAnimComp(comp->children[i], state, defstate, composed, out);
}

static void BuildAnimTransforms(VESSEL *vessel, AnimXformMap &out)
{
    ANIMATION *anim = nullptr;
    const UINT nanim = vessel->GetAnimPtr(&anim);
    if (!anim || nanim == 0) return;
    for (UINT a = 0; a < nanim; ++a) {
        const ANIMATION &A = anim[a];
        if (A.state == A.defstate) continue;  // mesh already at authored pose
        for (UINT c = 0; c < A.ncomp; ++c) {
            ANIMATIONCOMP *comp = A.comp[c];
            if (!comp || comp->parent) continue;  // roots only; children recursed
            ProcessAnimComp(comp, A.state, A.defstate, kIdentity4, out);
        }
    }
}

} // anonymous namespace

// Port of D3D9Client vVessel::Render mesh-visibility filter (VVessel.cpp:739-757).
// Returns true when the mesh should be drawn in the current pass.
// `internalpass` distinguishes the external/VC pass; `bCockpit` is true when the
// focus vessel is in internal view; `bVC` when cockpit mode is COCKPIT_VIRTUAL.
static bool ShouldRenderMesh(WORD vismode, bool internalpass, bool bCockpit, bool bVC)
{
	if (vismode == 0) return false;                           // MESHVIS_NEVER

	if (!internalpass) {
		if (vismode == MESHVIS_VC) return false;              // pure-VC mesh never external
		if (!(vismode & MESHVIS_EXTPASS) && bCockpit) return false;
	}

	if (bCockpit) {
		if (internalpass && (vismode & MESHVIS_EXTPASS)) return false;
		if (!(vismode & MESHVIS_COCKPIT)) {
			if (!bVC || !(vismode & MESHVIS_VC)) return false;
		}
	} else {
		if (!(vismode & MESHVIS_EXTERNAL)) return false;
	}
	return true;
}

CachedMesh::~CachedMesh() {
	for (auto &g : groups) {
		if (g.vao)    glDeleteVertexArrays(1, &g.vao);
		if (g.vbo)    glDeleteBuffers(1, &g.vbo);
		if (g.tbnVbo) glDeleteBuffers(1, &g.tbnVbo);
		if (g.ebo)    glDeleteBuffers(1, &g.ebo);
	}
}

// Compute per-vertex tangents using the classic Lengyel-2001 method:
// accumulate (tangent, bitangent) from every triangle that references a
// vertex, then Gram-Schmidt orthogonalise against the vertex normal and
// store the bitangent handedness in .w. Returns a flat array of vec4s
// sized to grp->nVtx.
static std::vector<float> ComputeTangents(const MESHGROUPEX *grp)
{
	const DWORD n = grp->nVtx;
	std::vector<float> tAcc(n * 3, 0.0f);
	std::vector<float> bAcc(n * 3, 0.0f);

	for (DWORD i = 0; i + 2 < grp->nIdx; i += 3) {
		WORD i0 = grp->Idx[i + 0];
		WORD i1 = grp->Idx[i + 1];
		WORD i2 = grp->Idx[i + 2];
		if (i0 >= n || i1 >= n || i2 >= n) continue;

		const NTVERTEX &v0 = grp->Vtx[i0];
		const NTVERTEX &v1 = grp->Vtx[i1];
		const NTVERTEX &v2 = grp->Vtx[i2];

		float e1x = v1.x - v0.x, e1y = v1.y - v0.y, e1z = v1.z - v0.z;
		float e2x = v2.x - v0.x, e2y = v2.y - v0.y, e2z = v2.z - v0.z;
		float du1 = v1.tu - v0.tu, dv1 = v1.tv - v0.tv;
		float du2 = v2.tu - v0.tu, dv2 = v2.tv - v0.tv;

		float denom = du1 * dv2 - du2 * dv1;
		if (std::fabs(denom) < 1e-8f) continue;
		float r = 1.0f / denom;

		float tx = (dv2 * e1x - dv1 * e2x) * r;
		float ty = (dv2 * e1y - dv1 * e2y) * r;
		float tz = (dv2 * e1z - dv1 * e2z) * r;
		float bx = (-du2 * e1x + du1 * e2x) * r;
		float by = (-du2 * e1y + du1 * e2y) * r;
		float bz = (-du2 * e1z + du1 * e2z) * r;

		const WORD idx[3] = { i0, i1, i2 };
		for (int k = 0; k < 3; k++) {
			DWORD vi = idx[k];
			tAcc[vi * 3 + 0] += tx; tAcc[vi * 3 + 1] += ty; tAcc[vi * 3 + 2] += tz;
			bAcc[vi * 3 + 0] += bx; bAcc[vi * 3 + 1] += by; bAcc[vi * 3 + 2] += bz;
		}
	}

	std::vector<float> out(n * 4, 0.0f);
	for (DWORD v = 0; v < n; v++) {
		float nx = grp->Vtx[v].nx, ny = grp->Vtx[v].ny, nz = grp->Vtx[v].nz;
		float tx = tAcc[v * 3 + 0], ty = tAcc[v * 3 + 1], tz = tAcc[v * 3 + 2];

		// Gram-Schmidt orthogonalisation of tangent against the vertex normal.
		float nt = nx * tx + ny * ty + nz * tz;
		tx -= nx * nt; ty -= ny * nt; tz -= nz * nt;
		float len = std::sqrt(tx * tx + ty * ty + tz * tz);
		if (len > 1e-6f) { float inv = 1.0f / len; tx *= inv; ty *= inv; tz *= inv; }
		else { tx = 1.0f; ty = 0.0f; tz = 0.0f; }

		// Handedness = sign(dot(cross(normal, tangent), accumulated bitangent))
		float cx = ny * tz - nz * ty;
		float cy = nz * tx - nx * tz;
		float cz = nx * ty - ny * tx;
		float bxA = bAcc[v * 3 + 0], byA = bAcc[v * 3 + 1], bzA = bAcc[v * 3 + 2];
		float h = (cx * bxA + cy * byA + cz * bzA) < 0.0f ? -1.0f : 1.0f;

		out[v * 4 + 0] = tx;
		out[v * 4 + 1] = ty;
		out[v * 4 + 2] = tz;
		out[v * 4 + 3] = h;
	}
	return out;
}

void OGLvVessel::InitShared(ShaderMgr *shaderMgr, const std::string &texturePath)
{
	if (s_sharedInitialized) return;
	s_sharedInitialized = true;
	s_shaderMgr = shaderMgr;

	s_vesselShader = shaderMgr->LoadProgram("vessel", "vessel.vert", "vessel.frag");
	s_pbrShader = shaderMgr->LoadProgram("vessel_pbr", "vessel_pbr.vert", "vessel_pbr.frag");
	s_exhaustShader = shaderMgr->LoadProgram("exhaust", "exhaust.vert", "exhaust.frag");
	s_shadowShader = shaderMgr->LoadProgram("shadow_cast", "shadow_cast.vert", "shadow_cast.frag");

	// The Material UBO is shared by both vessel programs — sized to match
	// shaders/include/material.glsl.inc and bound to UBO::Material.
	s_materialUBO = shaderMgr->CreateUBO(UBO::Material, sizeof(UBOMaterialData));

	// Shared 1024x1024 depth target for the vessel shadow pre-pass.
	s_shadowMap = new OGLShadowMap();
	if (!s_shadowMap->Init(1024)) {
		delete s_shadowMap;
		s_shadowMap = nullptr;
	}

	// Load exhaust texture
	std::string path = texturePath + "Exhaust.dds";
	if (FileExists(path.c_str()))
		s_exhaustTexture = OGLTexture::LoadDDS(path.c_str());

	// Exhaust billboard geometry (positions filled per-exhaust, per-frame).
	// Two quads:
	//   vtx 0..3 — core cone (narrow at the tip, sits on the plume axis)
	//   vtx 4..7 — flare halo (larger camera-aligned quad centred at the
	//              nozzle), gives the "bright bulb" look Windows ships
	//              (see D3D9Client/D3D9Effect.cpp:778-816).
	// UV atlas: core sits in the left strip of Exhaust.dds (U 0.01..0.24),
	// flare in the upper-right region (U 0.50..1.00, V 0..0.50).
	float verts[] = {
		0,0,0, 0.24f, 0.0f,
		0,0,0, 0.24f, 1.0f,
		0,0,0, 0.01f, 0.0f,
		0,0,0, 0.01f, 1.0f,
		0,0,0, 0.50390625f, 0.00390625f,
		0,0,0, 0.99609375f, 0.00390625f,
		0,0,0, 0.50390625f, 0.49609375f,
		0,0,0, 0.99609375f, 0.49609375f,
	};
	unsigned int idx[] = {
		0,1,2,  3,2,1,     // core
		4,5,6,  7,6,5,     // flare
	};

	glGenVertexArrays(1, &s_exhaustVAO);
	glGenBuffers(1, &s_exhaustVBO);
	glGenBuffers(1, &s_exhaustEBO);
	glBindVertexArray(s_exhaustVAO);
	glBindBuffer(GL_ARRAY_BUFFER, s_exhaustVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_exhaustEBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
	glBindVertexArray(0);
}

void OGLvVessel::ReleaseShared()
{
	MeshRegistry::Instance().Clear();
	MaterialStore::Instance().Clear();
	s_fallbackMeshes.clear();
	if (s_exhaustVAO) { glDeleteVertexArrays(1, &s_exhaustVAO); s_exhaustVAO = 0; }
	if (s_exhaustVBO) { glDeleteBuffers(1, &s_exhaustVBO); s_exhaustVBO = 0; }
	if (s_exhaustEBO) { glDeleteBuffers(1, &s_exhaustEBO); s_exhaustEBO = 0; }
	if (s_materialUBO && s_shaderMgr) { s_shaderMgr->ReleaseUBO(s_materialUBO); s_materialUBO = 0; }
	delete s_shadowMap; s_shadowMap = nullptr;
	delete s_exhaustTexture; s_exhaustTexture = nullptr;
	s_sharedInitialized = false;
}

OGLvVessel::OGLvVessel(OBJHANDLE hObj, ShaderMgr *shaderMgr)
	: OGLvObject(hObj, shaderMgr)
{
}

OGLvVessel::~OGLvVessel() {}

CachedMesh *OGLvVessel::GetOrCreateMeshCache(MESHHANDLE hMesh)
{
	MeshRegistry &reg = MeshRegistry::Instance();
	if (CachedMesh *hit = reg.Acquire(hMesh))
		return hit;

	// Miss or dirty — rebuild the VBO/EBO/VAO tuple for every group.
	CachedMesh *cached = new CachedMesh();
	DWORD nGrp = oapiMeshGroupCount(hMesh);
	for (DWORD g = 0; g < nGrp; g++) {
		MESHGROUPEX *grp = oapiMeshGroupEx(hMesh, g);
		if (!grp || !grp->nVtx || !grp->nIdx) {
			cached->groups.push_back({0, 0, 0, 0, 0, false});
			continue;
		}
		CachedMeshGroup cmg{};
		cmg.indexCount = (int)grp->nIdx;
		glGenVertexArrays(1, &cmg.vao);
		glGenBuffers(1, &cmg.vbo);
		glGenBuffers(1, &cmg.tbnVbo);
		glGenBuffers(1, &cmg.ebo);
		glBindVertexArray(cmg.vao);

		glBindBuffer(GL_ARRAY_BUFFER, cmg.vbo);
		glBufferData(GL_ARRAY_BUFFER, grp->nVtx * sizeof(NTVERTEX), grp->Vtx, GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(NTVERTEX), (void*)0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(NTVERTEX), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(NTVERTEX), (void*)(6 * sizeof(float)));
		glEnableVertexAttribArray(2);

		// Tangent stream: vec4(tangent.xyz, bitangent sign) per vertex.
		std::vector<float> tangents = ComputeTangents(grp);
		glBindBuffer(GL_ARRAY_BUFFER, cmg.tbnVbo);
		glBufferData(GL_ARRAY_BUFFER, tangents.size() * sizeof(float), tangents.data(), GL_STATIC_DRAW);
		glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(3);
		cmg.hasTangent = true;

		std::vector<unsigned int> indices(grp->nIdx);
		for (DWORD i = 0; i < grp->nIdx; i++) indices[i] = (unsigned int)grp->Idx[i];
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cmg.ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, grp->nIdx * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

		glBindVertexArray(0);
		cached->groups.push_back(cmg);
	}
	reg.Store(hMesh, cached);
	fprintf(stderr, "[OGLvVessel] Rebuilt mesh %p: %d groups\n", hMesh, (int)nGrp);
	return cached;
}

void OGLvVessel::Render(const float *vp, const VECTOR3 &camPos, const VECTOR3 &sunPos)
{
	if (!s_vesselShader) return;

	VESSEL *vessel = oapiGetVesselInterface(m_hObj);
	if (!vessel) return;

	VECTOR3 vpos;
	oapiGetGlobalPos(m_hObj, &vpos);
	double vx = vpos.x - camPos.x, vy = vpos.y - camPos.y, vz = vpos.z - camPos.z;
	double vdist = sqrt(vx * vx + vy * vy + vz * vz);

	if (vdist > 1e5) return;

	MATRIX3 vrot;
	oapiGetRotationMatrix(m_hObj, &vrot);

	// Sun direction
	double sdx = sunPos.x - vpos.x, sdy = sunPos.y - vpos.y, sdz = sunPos.z - vpos.z;
	double sdist = sqrt(sdx * sdx + sdy * sdy + sdz * sdz);
	if (sdist > 0) { sdx /= sdist; sdy /= sdist; sdz /= sdist; }
	float sunDir[3] = {(float)sdx, (float)sdy, (float)sdz};

	// Distance normalization
	double normDist = vdist, scale = 1.0;
	if (vdist > 1000.0) { normDist = 1000.0; scale = normDist / vdist; }
	double nvx = vx * scale, nvy = vy * scale, nvz = vz * scale;

	// Prefer the PBR pipeline — M3 + M8 have the full Material UBO and real
	// TBN frames in place, and M9 now supplies the prefiltered environment
	// cubemap. We still fall back to the legacy shader if, for any reason,
	// the PBR link dropped out.
	GLuint activeShader = s_pbrShader ? s_pbrShader : s_vesselShader;
	const bool pbrActive = (activeShader == s_pbrShader);

	// Cockpit-view state: F1 toggles camera internal/external; when internal
	// and this vessel is the focus we run a second pass with internalpass=true
	// so MESHVIS_VC/MESHVIS_COCKPIT meshes can draw. Planetarium/env/shadow
	// cameras never enter this path because they're not the "main" view.
	const bool bCockpit = (oapiCameraInternal() && (m_hObj == oapiGetFocusObject()));
	const bool bVC      = (bCockpit && (oapiCockpitMode() == COCKPIT_VIRTUAL));
	const int  nPasses  = bCockpit ? 2 : 1;

	// Cache VC MFD surface bindings for this frame. For each registered MFD
	// the client returns a SURFHANDLE holding the painted display and a spec
	// identifying the mesh/group it should replace. When we hit (m, g) that
	// matches, we bind the MFD surface instead of the group's base texture —
	// same approach as D3D9Client's mesh->SetMFDScreenId(g, 1+mfd) plumbing.
	struct MFDBinding { DWORD nmesh, ngroup; OGLSurface *surf; };
	MFDBinding mfdBindings[MAXMFD];
	int numMFDBindings = 0;
	if (bVC && s_gc) {
		for (int i = 0; i < MAXMFD; i++) {
			const VCMFDSPEC *spec = nullptr;
			SURFHANDLE s = s_gc->GetVCMFDSurface(i, &spec);
			if (s && spec) {
				mfdBindings[numMFDBindings].nmesh  = spec->nmesh;
				mfdBindings[numMFDBindings].ngroup = spec->ngroup;
				mfdBindings[numMFDBindings].surf   = (OGLSurface*)s;
				numMFDBindings++;
			}
		}
	}

	// Cache the VC HUD binding. Unlike MFDs the HUD is an additive overlay:
	// the group draws once with its base texture (window glass), then we
	// re-draw the same geometry with the HUD surface and an additive blend
	// so the transparent sketchpad background leaves the glass visible and
	// only the painted strokes add colour — equivalent to the 0x100 screen
	// type D3D9Client tags its HUD group with (VVessel.cpp:802-806).
	DWORD hudMesh = (DWORD)-1, hudGroup = (DWORD)-1;
	OGLSurface *hudSurf = nullptr;
	if (bVC && s_gc) {
		const VCHUDSPEC *hudspec = nullptr;
		SURFHANDLE s = s_gc->GetVCHUDSurface(&hudspec);
		if (s && hudspec) {
			hudMesh  = hudspec->nmesh;
			hudGroup = hudspec->ngroup;
			hudSurf  = (OGLSurface*)s;
		}
	}

	// ----- Shadow pre-pass (PBR path only) ----------------------------------
	// Render the vessel's meshes into the shared shadow depth map from the
	// sun's point of view. Both passes work in the distance-normalised frame
	// the main pass uses, so the uModel matrix is identical between them.
	const bool shadowsReady = pbrActive && s_shadowMap && s_shadowShader;
	if (shadowsReady) {
		VECTOR3 sunUnit    = { (double)sunDir[0], (double)sunDir[1], (double)sunDir[2] };
		VECTOR3 tgtScaled  = { nvx, nvy, nvz };
		double  halfExtent = std::max(1.0, vessel->GetSize() * scale * 2.0);
		double  sunDistOrth = halfExtent * 4.0;
		double  farDistOrth = sunDistOrth * 2.0 + halfExtent;
		s_shadowMap->BuildLightMatrix(sunUnit, tgtScaled, halfExtent, sunDistOrth, farDistOrth);

		s_shadowMap->BeginPass();
		glUseProgram(s_shadowShader);
		glUniformMatrix4fv(s_shaderMgr->GetUniformLoc(s_shadowShader, "uLightVP"),
		                   1, GL_FALSE, s_shadowMap->GetLightVP());

		DWORD nMeshShadow = vessel->GetMeshCount();
		for (DWORD m = 0; m < nMeshShadow; m++) {
			// Sun's POV is always external — filter out VC/cockpit-only meshes
			// so we don't pay shadow cost for geometry that's never lit (and
			// avoid self-shadowing artefacts from interior panels).
			WORD vismode = vessel->GetMeshVisibilityMode(m);
			if (!ShouldRenderMesh(vismode, /*internalpass=*/false, /*bCockpit=*/false, /*bVC=*/false))
				continue;

			// Same fallback as the main pass — look up the cache by the stored
			// mesh name, not the vessel class name. The shadow pass only reads
			// (never populates) the cache so a missing entry just skips the
			// shadow for this frame.
			MESHHANDLE hMesh = vessel->GetMeshTemplate(m);
			if (!hMesh) {
				const char *meshName = vessel->GetMeshName(m);
				if (meshName && *meshName) {
					auto it = s_fallbackMeshes.find(meshName);
					if (it != s_fallbackMeshes.end()) hMesh = it->second;
				}
			}
			if (!hMesh) continue;

			VECTOR3 meshOfs = {0, 0, 0};
			vessel->GetMeshOffset(m, meshOfs);
			CachedMesh *cached = GetOrCreateMeshCache(hMesh);
			if (!cached) continue;

			double ox = vrot.m11 * meshOfs.x + vrot.m12 * meshOfs.y + vrot.m13 * meshOfs.z;
			double oy = vrot.m21 * meshOfs.x + vrot.m22 * meshOfs.y + vrot.m23 * meshOfs.z;
			double oz = vrot.m31 * meshOfs.x + vrot.m32 * meshOfs.y + vrot.m33 * meshOfs.z;
			float tx = (float)(nvx + ox * scale);
			float ty = (float)(nvy + oy * scale);
			float tz = (float)(nvz + oz * scale);
			float sS = (float)scale;
			float model[16] = {
				(float)(vrot.m11 * sS), (float)(vrot.m21 * sS), (float)(vrot.m31 * sS), 0.0f,
				(float)(vrot.m12 * sS), (float)(vrot.m22 * sS), (float)(vrot.m32 * sS), 0.0f,
				(float)(vrot.m13 * sS), (float)(vrot.m23 * sS), (float)(vrot.m33 * sS), 0.0f,
				tx, ty, tz, 1.0f
			};
			glUniformMatrix4fv(s_shaderMgr->GetUniformLoc(s_shadowShader, "uModel"),
			                   1, GL_FALSE, model);

			for (const CachedMeshGroup &cmg : cached->groups) {
				if (!cmg.vao || cmg.indexCount == 0) continue;
				glBindVertexArray(cmg.vao);
				glDrawElements(GL_TRIANGLES, cmg.indexCount, GL_UNSIGNED_INT, 0);
			}
		}
		glBindVertexArray(0);
		glUseProgram(0);
		s_shadowMap->EndPass();
	}

	// ----- Main pass ---------------------------------------------------------
	glUseProgram(activeShader);
	glUniformMatrix4fv(s_shaderMgr->GetUniformLoc(activeShader, "uViewProj"), 1, GL_FALSE, vp);
	glUniform3fv(s_shaderMgr->GetUniformLoc(activeShader, "uSunDir"), 1, sunDir);

	// Shadow-map sampling: bind the depth target + its light VP on the PBR
	// shader. shadowsReady implies pbrActive so we only do this on the PBR
	// path; the legacy shader doesn't declare uShadowMap.
	if (shadowsReady) {
		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, s_shadowMap->GetDepthTexture());
		glUniform1i(s_shaderMgr->GetUniformLoc(activeShader, "uShadowMap"), 3);
		glUniformMatrix4fv(s_shaderMgr->GetUniformLoc(activeShader, "uLightVP"),
		                   1, GL_FALSE, s_shadowMap->GetLightVP());
		glActiveTexture(GL_TEXTURE0);
	}

	// Bind the IBL environment cubemap once per vessel. The legacy shader
	// doesn't sample samplerCubes but the driver is happy with an unused
	// sampler bound — we just leave matHasEnvMap=0 so the PBR fragment
	// shader skips the IBL lookup when the bake hasn't finished yet.
	const bool envReady = pbrActive && s_envMap && s_envMap->IsReady();
	if (pbrActive) {
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_CUBE_MAP,
		              envReady ? s_envMap->GetPrefilterCube() : 0);
		glUniform1i(s_shaderMgr->GetUniformLoc(activeShader, "uEnvMap"), 2);
		glActiveTexture(GL_TEXTURE0);
	}

	// Orbiter's mesh files were authored for D3D's clockwise-is-front
	// convention; OpenGL defaults to counter-clockwise-is-front, so a
	// glCullFace(GL_BACK) was flipping the hull — the exterior faces got
	// culled and we ended up staring at the interior of the opposite side
	// of the ship (look-from-above showed the inside of the belly, and
	// vice versa — #69). Swap the front-face winding for the duration of
	// the vessel pass and restore CCW before returning so procedural
	// geometry (full-screen quads, spheres) keeps its expected winding.
	glFrontFace(GL_CW);

	// Evaluate per-(mesh, group) animation transforms once per frame. Empty
	// when the vessel has no animations or all states match their defaults.
	AnimXformMap animTransforms;
	BuildAnimTransforms(vessel, animTransforms);

	DWORD nMesh = vessel->GetMeshCount();
	// Pass 0: external (hull, EXTPASS bits even while in cockpit view).
	// Pass 1: internal/VC — only runs when the focus vessel is in internal
	// view. Both passes share the same shader state; depth buffer is kept
	// across passes so any EXTPASS geometry (hull seen through windows)
	// occludes VC interior correctly through the usual z-test.
	for (int pass = 0; pass < nPasses; pass++) {
		const bool internalpass = (pass == 1);

	for (DWORD m = 0; m < nMesh; m++) {
		WORD vismode = vessel->GetMeshVisibilityMode(m);
		if (!ShouldRenderMesh(vismode, internalpass, bCockpit, bVC)) continue;

		// AddMesh(hMesh, ...) stores the preloaded handle, but AddMesh(name, ofs)
		// leaves `hMesh = NULL` and keeps only the filename. The D3D9 client
		// calls `CopyMeshFromTemplate` in that case (VVessel.cpp:298) which
		// loads from `meshname`. Previously we fell back to
		// `oapiLoadMeshGlobal(vessel->GetClassName())` — but the class name
		// has nothing to do with the missing mesh slot, so on every Atlantis
		// mission the optional `shuttle_eva_plat` / cargo slot re-loaded the
		// orbiter mesh and rendered it at the platform offset, producing a
		// second shuttle hovering next to the real one. Use the stored mesh
		// name instead; fall through and skip when both are missing.
		MESHHANDLE hMesh = vessel->GetMeshTemplate(m);
		if (!hMesh) {
			const char *meshName = vessel->GetMeshName(m);
			if (meshName && *meshName) {
				auto it = s_fallbackMeshes.find(meshName);
				if (it != s_fallbackMeshes.end()) {
					hMesh = it->second;
				} else {
					hMesh = oapiLoadMeshGlobal(meshName);
					s_fallbackMeshes[meshName] = hMesh;
				}
			}
			if (!hMesh) continue;
		}

		VECTOR3 meshOfs = {0, 0, 0};
		vessel->GetMeshOffset(m, meshOfs);
		CachedMesh *cached = GetOrCreateMeshCache(hMesh);
		if (!cached) continue;

		double ox = vrot.m11 * meshOfs.x + vrot.m12 * meshOfs.y + vrot.m13 * meshOfs.z;
		double oy = vrot.m21 * meshOfs.x + vrot.m22 * meshOfs.y + vrot.m23 * meshOfs.z;
		double oz = vrot.m31 * meshOfs.x + vrot.m32 * meshOfs.y + vrot.m33 * meshOfs.z;
		float tx = (float)(nvx + ox * scale), ty = (float)(nvy + oy * scale), tz = (float)(nvz + oz * scale);
		float s = (float)scale;

		const Mat4 rootModel = {
			(float)(vrot.m11 * s), (float)(vrot.m21 * s), (float)(vrot.m31 * s), 0.0f,
			(float)(vrot.m12 * s), (float)(vrot.m22 * s), (float)(vrot.m32 * s), 0.0f,
			(float)(vrot.m13 * s), (float)(vrot.m23 * s), (float)(vrot.m33 * s), 0.0f,
			tx, ty, tz, 1.0f
		};
		const GLint uModelLoc = s_shaderMgr->GetUniformLoc(activeShader, "uModel");

		DWORD nTex = oapiMeshTextureCount(hMesh);

		for (DWORD g = 0; g < (DWORD)cached->groups.size(); g++) {
			CachedMeshGroup &cmg = cached->groups[g];
			if (!cmg.vao || cmg.indexCount == 0) continue;

			// Compose the per-group model matrix: rootModel * animTransform
			// when this group has an active animation, plain rootModel
			// otherwise. The map is usually empty or small (~15 entries for
			// the DeltaGlider's gear) so the lookup is cheap.
			auto animIt = animTransforms.find(AnimKey(m, g));
			const Mat4 groupModel = (animIt != animTransforms.end())
				? MatMul(rootModel, animIt->second)
				: rootModel;
			glUniformMatrix4fv(uModelLoc, 1, GL_FALSE, groupModel.data());

			MESHGROUPEX *grp = oapiMeshGroupEx(hMesh, g);

			// Build the std140 Material block: defaults + Orbiter MESH
			// material + any runtime override installed by vessel modules
			// through clbkSetMeshMaterialEx.
			UBOMaterialData matData;
			BuildMaterialData(matData, hMesh, g);
			if (grp && grp->MtrlIdx > 0) {
				MaterialStore::Instance().Apply(
					matData, (DEVMESHHANDLE)hMesh, grp->MtrlIdx - 1);
			}

			// Diffuse texture sampling is driven by matHasDiffuse inside the
			// shader; the sampler itself still needs a traditional binding
			// (GLSL 410 forbids opaque types in uniform blocks).
			bool hasTexture = false;
			OGLSurface *texOverride = nullptr;
			if (internalpass && bVC) {
				for (int k = 0; k < numMFDBindings; k++) {
					if (mfdBindings[k].nmesh == m && mfdBindings[k].ngroup == g) {
						texOverride = mfdBindings[k].surf;
						break;
					}
				}
			}
			if (texOverride) {
				GLuint texId = texOverride->GetTexture();
				if (texId) {
					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_2D, texId);
					GLint texLoc = s_shaderMgr->GetUniformLoc(activeShader,
						activeShader == s_pbrShader ? "uDiffuseTex" : "uTexture");
					glUniform1i(texLoc, 0);
					hasTexture = true;
				}
			} else if (grp && grp->TexIdx < nTex) {
				// MESHGROUPEX::TexIdx is stored 0-based by the mesh loader
				// (Src/Orbiter/Mesh.cpp:868-869 subtracts 1 from the file's
				// 1-based TEXTURE index; SPEC_DEFAULT = UINT_MAX flags "no
				// texture"). The public oapiGetTextureHandle API is 1-based
				// — it calls Mesh::GetTexture(texidx - 1) internally — so
				// we need to shift back up by one when dereferencing. The
				// previous `TexIdx > 0 && TexIdx <= nTex` guard on the raw
				// value happened to bind groups with file-TEXTURE >= 2 to
				// the *wrong* texture and silently dropped file-TEXTURE 1
				// groups entirely (issue #96, Atlantis fuselage).
				SURFHANDLE hSurf = oapiGetTextureHandle(hMesh, grp->TexIdx + 1);
				if (hSurf) {
					OGLSurface *surf = (OGLSurface*)hSurf;
					GLuint texId = surf->GetTexture();
					if (texId) {
						glActiveTexture(GL_TEXTURE0);
						glBindTexture(GL_TEXTURE_2D, texId);
						GLint texLoc = s_shaderMgr->GetUniformLoc(activeShader,
							activeShader == s_pbrShader ? "uDiffuseTex" : "uTexture");
						glUniform1i(texLoc, 0);
						hasTexture = true;
					}
				}
			}
			matData.hasDiffuse = hasTexture ? 1 : 0;
			matData.hasTangent = cmg.hasTangent ? 1 : 0;
			matData.hasEnvMap  = envReady ? 1 : 0;

			s_shaderMgr->UpdateUBO(s_materialUBO, sizeof(matData), &matData);

			glBindVertexArray(cmg.vao);
			glDrawElements(GL_TRIANGLES, cmg.indexCount, GL_UNSIGNED_INT, 0);

			// VC HUD overlay: re-draw the matching group additively with the
			// HUD surface on top. The sketchpad background is transparent so
			// only stroked pixels contribute; the underlying window glass
			// (just drawn above) stays visible.
			if (internalpass && bVC && hudSurf && m == hudMesh && g == hudGroup) {
				GLuint hudTexId = hudSurf->GetTexture();
				if (hudTexId) {
					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_2D, hudTexId);
					GLint texLoc = s_shaderMgr->GetUniformLoc(activeShader,
						activeShader == s_pbrShader ? "uDiffuseTex" : "uTexture");
					glUniform1i(texLoc, 0);

					// Emissive-only material so the HUD strokes aren't
					// modulated by diffuse lighting — HUD should read the
					// same at any cockpit orientation.
					UBOMaterialData hudMat = matData;
					hudMat.hasDiffuse = 1;
					hudMat.hasEnvMap  = 0;
					s_shaderMgr->UpdateUBO(s_materialUBO, sizeof(hudMat), &hudMat);

					glEnable(GL_BLEND);
					glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // additive, alpha-weighted
					glDepthMask(GL_FALSE);
					glDrawElements(GL_TRIANGLES, cmg.indexCount, GL_UNSIGNED_INT, 0);
					glDepthMask(GL_TRUE);
					glDisable(GL_BLEND);
				}
			}
		}
	}
	} // end pass loop
	glFrontFace(GL_CCW);  // restore OGL default for subsequent passes
	glBindVertexArray(0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);

	// Render exhaust plumes
	double sc = (vdist > 1000.0) ? 1000.0 / vdist : 1.0;
	MATRIX3 vr;
	oapiGetRotationMatrix(m_hObj, &vr);
	RenderExhausts(vessel, vr, (float)(vx * sc), (float)(vy * sc), (float)(vz * sc), (float)sc, vp, camPos);
}

void OGLvVessel::RenderExhausts(VESSEL *vessel, const MATRIX3 &vrot,
                                 float tx, float ty, float tz, float scale,
                                 const float *vp, const VECTOR3 &camPos)
{
	if (!s_exhaustShader || !s_exhaustVAO || !s_exhaustTexture) return;

	DWORD nExhaust = vessel->GetExhaustCount();
	if (nExhaust == 0) return;

	glUseProgram(s_exhaustShader);
	glUniformMatrix4fv(s_shaderMgr->GetUniformLoc(s_exhaustShader, "uViewProj"), 1, GL_FALSE, vp);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, s_exhaustTexture->texId);
	glUniform1i(s_shaderMgr->GetUniformLoc(s_exhaustShader, "uTexture"), 0);
	// Animated flicker (issue #102): the shader modulates brightness and
	// alpha by a mix of a sinusoid and cheap UV turbulence; we feed it the
	// sim time + a per-thruster phase so engines don't pulse in lockstep.
	const float simTime = (float)oapiGetSimTime();
	const GLint uTimeLoc  = s_shaderMgr->GetUniformLoc(s_exhaustShader, "uTime");
	const GLint uPhaseLoc = s_shaderMgr->GetUniformLoc(s_exhaustShader, "uPhase");
	glUniform1f(uTimeLoc, simTime);

	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	glDepthMask(GL_FALSE);

	float cdx = -tx, cdy = -ty, cdz = -tz;
	float cdlen = sqrtf(cdx * cdx + cdy * cdy + cdz * cdz);
	if (cdlen > 0) { cdx /= cdlen; cdy /= cdlen; cdz /= cdlen; }

	for (DWORD i = 0; i < nExhaust; i++) {
		double level = vessel->GetExhaustLevel(i);
		if (level < 0.01) continue;
		EXHAUSTSPEC es;
		if (!vessel->GetExhaustSpec(i, &es)) continue;
		if (!es.lpos || !es.ldir) continue;

		// Per-thruster phase + length jitter. The phase is a deterministic
		// hash of the index so a given engine always animates the same way
		// frame to frame; the length wobble rides on a slow ~3 Hz sine so
		// the plume feels like it's breathing rather than just blinking.
		const float phase = (float)i * 2.3987f;
		const float jitter = 1.0f + 0.06f * sinf(simTime * 3.0f + phase);
		glUniform1f(uPhaseLoc, phase);

		float lsz = (float)(es.lsize * level * scale) * jitter;
		float wsz = (float)(es.wsize * level * scale);
		if (lsz < 0.001f) continue;

		// Plume origin (lpos, offset back by lofs along the exhaust
		// direction to match D3D9's `ref = lpos - ldir*lofs`).
		VECTOR3 lp = *es.lpos, ld = *es.ldir;
		VECTOR3 refLocal = {
			lp.x - ld.x * es.lofs,
			lp.y - ld.y * es.lofs,
			lp.z - ld.z * es.lofs
		};
		float ex = (float)(vrot.m11 * refLocal.x + vrot.m12 * refLocal.y + vrot.m13 * refLocal.z) * scale + tx;
		float ey = (float)(vrot.m21 * refLocal.x + vrot.m22 * refLocal.y + vrot.m23 * refLocal.z) * scale + ty;
		float ez = (float)(vrot.m31 * refLocal.x + vrot.m32 * refLocal.y + vrot.m33 * refLocal.z) * scale + tz;
		// Exhaust direction in world (edir = -ldir, normalised).
		float dx = -(float)(vrot.m11 * ld.x + vrot.m12 * ld.y + vrot.m13 * ld.z);
		float dy = -(float)(vrot.m21 * ld.x + vrot.m22 * ld.y + vrot.m23 * ld.z);
		float dz = -(float)(vrot.m31 * ld.x + vrot.m32 * ld.y + vrot.m33 * ld.z);

		// sdir = camera × exhaust (perpendicular to both, used to
		// fatten the core quad) — normalised.
		float sx = cdy * dz - cdz * dy, sy = cdz * dx - cdx * dz, sz = cdx * dy - cdy * dx;
		float slen = sqrtf(sx * sx + sy * sy + sz * sz);
		if (slen < 1e-6f) { sx = 1; sy = 0; sz = 0; slen = 1; }
		sx /= slen; sy /= slen; sz /= slen;

		// tdir = camera × sdir. Camera-aligned, used to build the flare
		// quad so it always faces the viewer.
		float ttx = cdy * sz - cdz * sy;
		float tty = cdz * sx - cdx * sz;
		float ttz = cdx * sy - cdy * sx;
		float tlen = sqrtf(ttx*ttx + tty*tty + ttz*ttz);
		if (tlen > 1e-6f) { ttx /= tlen; tty /= tlen; ttz /= tlen; }

		const float hw        = wsz * 0.5f;
		// D3D9 uses flarescale = 7; scale the same way but only to half
		// because our hw is already half-width.
		const float flare     = wsz * 3.5f;

		float verts[] = {
			// Core quad — same narrow cone as before.
			ex - sx * hw,             ey - sy * hw,             ez - sz * hw,             0.24f, 0.0f,
			ex + sx * hw,             ey + sy * hw,             ez + sz * hw,             0.24f, 1.0f,
			ex + dx * lsz - sx * hw * 0.2f, ey + dy * lsz - sy * hw * 0.2f, ez + dz * lsz - sz * hw * 0.2f, 0.01f, 0.0f,
			ex + dx * lsz + sx * hw * 0.2f, ey + dy * lsz + sy * hw * 0.2f, ez + dz * lsz + sz * hw * 0.2f, 0.01f, 1.0f,
			// Flare halo — camera-aligned quad centred at the nozzle,
			// 7× wide to match D3D9Client's bloom halo.
			ex - sx * flare + ttx * flare, ey - sy * flare + tty * flare, ez - sz * flare + ttz * flare, 0.50390625f, 0.00390625f,
			ex + sx * flare + ttx * flare, ey + sy * flare + tty * flare, ez + sz * flare + ttz * flare, 0.99609375f, 0.00390625f,
			ex - sx * flare - ttx * flare, ey - sy * flare - tty * flare, ez - sz * flare - ttz * flare, 0.50390625f, 0.49609375f,
			ex + sx * flare - ttx * flare, ey + sy * flare - tty * flare, ez + sz * flare - ttz * flare, 0.99609375f, 0.49609375f,
		};

		glBindVertexArray(s_exhaustVAO);
		glBindBuffer(GL_ARRAY_BUFFER, s_exhaustVBO);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
		glUniform1f(s_shaderMgr->GetUniformLoc(s_exhaustShader, "uAlpha"), (float)level);
		glDrawElements(GL_TRIANGLES, 12, GL_UNSIGNED_INT, 0);
	}

	glBindVertexArray(0);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	glUseProgram(0);
}

} // namespace ogl

#endif // !_WIN32
