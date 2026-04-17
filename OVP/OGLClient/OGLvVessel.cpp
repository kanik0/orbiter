// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef _WIN32

#include "OGLvVessel.h"
#include "OGLShaderMgr.h"
#include "OGLMaterial.h"
#include "OGLMeshRegistry.h"
#include "OGLTexture.h"
#include "OGLSurface.h"
#include "VesselAPI.h"
#include <cstdio>
#include <cmath>
#include <sys/stat.h>

namespace ogl {

// Static members
GLuint OGLvVessel::s_vesselShader = 0;
GLuint OGLvVessel::s_pbrShader = 0;
GLuint OGLvVessel::s_exhaustShader = 0;
GLuint OGLvVessel::s_exhaustVAO = 0, OGLvVessel::s_exhaustVBO = 0, OGLvVessel::s_exhaustEBO = 0;
GLuint OGLvVessel::s_materialUBO = 0;
OGLTexture *OGLvVessel::s_exhaustTexture = nullptr;
bool OGLvVessel::s_sharedInitialized = false;
ShaderMgr *OGLvVessel::s_shaderMgr = nullptr;
std::map<std::string, MESHHANDLE> OGLvVessel::s_fallbackMeshes;

static bool FileExists(const char *path) {
	struct stat st;
	return stat(path, &st) == 0;
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

	// The Material UBO is shared by both vessel programs — sized to match
	// shaders/include/material.glsl.inc and bound to UBO::Material.
	s_materialUBO = shaderMgr->CreateUBO(UBO::Material, sizeof(UBOMaterialData));

	// Load exhaust texture
	std::string path = texturePath + "Exhaust.dds";
	if (FileExists(path.c_str()))
		s_exhaustTexture = OGLTexture::LoadDDS(path.c_str());

	// Create exhaust billboard quad (vertices updated per-exhaust)
	float verts[] = {
		0,0,0, 0.24f,0,
		0,0,0, 0.24f,1,
		0,0,0, 0.01f,0,
		0,0,0, 0.01f,1,
	};
	unsigned int idx[] = {0,1,2, 3,2,1};

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

	// Force legacy shader for now — PBR needs more testing
	GLuint activeShader = s_vesselShader;
	glUseProgram(activeShader);
	glUniformMatrix4fv(s_shaderMgr->GetUniformLoc(activeShader, "uViewProj"), 1, GL_FALSE, vp);
	glUniform3fv(s_shaderMgr->GetUniformLoc(activeShader, "uSunDir"), 1, sunDir);

	DWORD nMesh = vessel->GetMeshCount();
	for (DWORD m = 0; m < nMesh; m++) {
		MESHHANDLE hMesh = vessel->GetMeshTemplate(m);
		if (!hMesh) {
			const char *className = vessel->GetClassName();
			if (className) {
				auto it = s_fallbackMeshes.find(className);
				if (it != s_fallbackMeshes.end()) {
					hMesh = it->second;
				} else {
					hMesh = oapiLoadMeshGlobal(className);
					s_fallbackMeshes[className] = hMesh;
					if (hMesh) fprintf(stderr, "[OGLvVessel] Loaded fallback mesh '%s': %u groups\n",
						className, oapiMeshGroupCount(hMesh));
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

		float model[16] = {
			(float)(vrot.m11 * s), (float)(vrot.m21 * s), (float)(vrot.m31 * s), 0.0f,
			(float)(vrot.m12 * s), (float)(vrot.m22 * s), (float)(vrot.m32 * s), 0.0f,
			(float)(vrot.m13 * s), (float)(vrot.m23 * s), (float)(vrot.m33 * s), 0.0f,
			tx, ty, tz, 1.0f
		};
		glUniformMatrix4fv(s_shaderMgr->GetUniformLoc(activeShader, "uModel"), 1, GL_FALSE, model);

		DWORD nTex = oapiMeshTextureCount(hMesh);

		for (DWORD g = 0; g < (DWORD)cached->groups.size(); g++) {
			CachedMeshGroup &cmg = cached->groups[g];
			if (!cmg.vao || cmg.indexCount == 0) continue;

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
			if (grp && grp->TexIdx > 0 && grp->TexIdx <= nTex) {
				SURFHANDLE hSurf = oapiGetTextureHandle(hMesh, grp->TexIdx);
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

			s_shaderMgr->UpdateUBO(s_materialUBO, sizeof(matData), &matData);

			glBindVertexArray(cmg.vao);
			glDrawElements(GL_TRIANGLES, cmg.indexCount, GL_UNSIGNED_INT, 0);
		}
	}
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

		float lsz = (float)(es.lsize * level * scale);
		float wsz = (float)(es.wsize * level * scale);
		if (lsz < 0.001f) continue;

		VECTOR3 lp = *es.lpos, ld = *es.ldir;
		float ex = (float)(vrot.m11 * lp.x + vrot.m12 * lp.y + vrot.m13 * lp.z) * scale + tx;
		float ey = (float)(vrot.m21 * lp.x + vrot.m22 * lp.y + vrot.m23 * lp.z) * scale + ty;
		float ez = (float)(vrot.m31 * lp.x + vrot.m32 * lp.y + vrot.m33 * lp.z) * scale + tz;
		float dx = -(float)(vrot.m11 * ld.x + vrot.m12 * ld.y + vrot.m13 * ld.z);
		float dy = -(float)(vrot.m21 * ld.x + vrot.m22 * ld.y + vrot.m23 * ld.z);
		float dz = -(float)(vrot.m31 * ld.x + vrot.m32 * ld.y + vrot.m33 * ld.z);

		float sx = cdy * dz - cdz * dy, sy = cdz * dx - cdx * dz, sz = cdx * dy - cdy * dx;
		float slen = sqrtf(sx * sx + sy * sy + sz * sz);
		if (slen < 1e-6f) { sx = 1; sy = 0; sz = 0; slen = 1; }
		sx /= slen; sy /= slen; sz /= slen;

		float hw = wsz * 0.5f;
		float verts[] = {
			ex - sx * hw, ey - sy * hw, ez - sz * hw, 0.24f, 0.0f,
			ex + sx * hw, ey + sy * hw, ez + sz * hw, 0.24f, 1.0f,
			ex + dx * lsz - sx * hw * 0.2f, ey + dy * lsz - sy * hw * 0.2f, ez + dz * lsz - sz * hw * 0.2f, 0.01f, 0.0f,
			ex + dx * lsz + sx * hw * 0.2f, ey + dy * lsz + sy * hw * 0.2f, ez + dz * lsz + sz * hw * 0.2f, 0.01f, 1.0f,
		};

		glBindVertexArray(s_exhaustVAO);
		glBindBuffer(GL_ARRAY_BUFFER, s_exhaustVBO);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
		glUniform1f(s_shaderMgr->GetUniformLoc(s_exhaustShader, "uAlpha"), (float)level);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	}

	glBindVertexArray(0);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	glUseProgram(0);
}

} // namespace ogl

#endif // !_WIN32
