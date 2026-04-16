// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef _WIN32

#include "OGLvPlanet.h"
#include "OGLShaderMgr.h"
#include "OGLTexture.h"
#include "OGLTile.h"
#include "OGLAtmosphere.h"
#include <cstdio>
#include <cmath>
#include <cstring>
#include <vector>
#include <sys/stat.h>

namespace ogl {

// Static members
GLuint OGLvPlanet::s_planetShader = 0;
GLuint OGLvPlanet::s_sphereVAO = 0, OGLvPlanet::s_sphereVBO = 0, OGLvPlanet::s_sphereEBO = 0;
int OGLvPlanet::s_sphereIndexCount = 0;
GLuint OGLvPlanet::s_texPlanetShader = 0;
GLuint OGLvPlanet::s_texSphereVAO = 0, OGLvPlanet::s_texSphereVBO = 0, OGLvPlanet::s_texSphereEBO = 0;
int OGLvPlanet::s_texSphereIndexCount = 0;
GLuint OGLvPlanet::s_ringShader = 0;
GLuint OGLvPlanet::s_ringVAO = 0, OGLvPlanet::s_ringVBO = 0, OGLvPlanet::s_ringEBO = 0;
int OGLvPlanet::s_ringIndexCount = 0;
OGLTexture *OGLvPlanet::s_ringTexture = nullptr;
bool OGLvPlanet::s_ringsInitialized = false;
bool OGLvPlanet::s_sharedInitialized = false;
ShaderMgr *OGLvPlanet::s_shaderMgr = nullptr;

static bool FileExists(const char *path) {
	struct stat st;
	return stat(path, &st) == 0;
}

// Build a unit sphere (no UVs, for flat-color rendering)
static void CreateSphere(int slices, int stacks, GLuint &vao, GLuint &vbo, GLuint &ebo, int &indexCount) {
	struct Vtx { float x, y, z, nx, ny, nz; };
	std::vector<Vtx> verts;
	std::vector<unsigned int> indices;
	for (int i = 0; i <= stacks; i++) {
		float phi = M_PI * i / stacks;
		for (int j = 0; j <= slices; j++) {
			float theta = 2.0f * M_PI * j / slices;
			float x = sinf(phi) * cosf(theta);
			float y = cosf(phi);
			float z = sinf(phi) * sinf(theta);
			verts.push_back({x, y, z, x, y, z});
		}
	}
	for (int i = 0; i < stacks; i++) {
		for (int j = 0; j < slices; j++) {
			int a = i * (slices + 1) + j, b = a + slices + 1;
			indices.push_back(a); indices.push_back(b); indices.push_back(a + 1);
			indices.push_back(a + 1); indices.push_back(b); indices.push_back(b + 1);
		}
	}
	indexCount = (int)indices.size();
	glGenVertexArrays(1, &vao); glGenBuffers(1, &vbo); glGenBuffers(1, &ebo);
	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vtx), verts.data(), GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
	glBindVertexArray(0);
}

// Build a UV-mapped sphere for textured rendering
static void CreateTexturedSphere(int slices, int stacks, GLuint &vao, GLuint &vbo, GLuint &ebo, int &indexCount) {
	struct TxVtx { float x, y, z, nx, ny, nz, u, v; };
	std::vector<TxVtx> verts;
	std::vector<unsigned int> indices;
	for (int i = 0; i <= stacks; i++) {
		float phi = M_PI * i / stacks;
		float v = (float)i / stacks;
		for (int j = 0; j <= slices; j++) {
			float theta = 2.0f * M_PI * j / slices;
			float u = (float)j / slices;
			float x = sinf(phi) * cosf(theta);
			float y = cosf(phi);
			float z = sinf(phi) * sinf(theta);
			verts.push_back({x, y, z, x, y, z, u, v});
		}
	}
	for (int i = 0; i < stacks; i++) {
		for (int j = 0; j < slices; j++) {
			int a = i * (slices + 1) + j, b = a + slices + 1;
			indices.push_back(a); indices.push_back(b); indices.push_back(a + 1);
			indices.push_back(a + 1); indices.push_back(b); indices.push_back(b + 1);
		}
	}
	indexCount = (int)indices.size();
	glGenVertexArrays(1, &vao); glGenBuffers(1, &vbo); glGenBuffers(1, &ebo);
	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(TxVtx), verts.data(), GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TxVtx), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(TxVtx), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(TxVtx), (void*)(6 * sizeof(float)));
	glEnableVertexAttribArray(2);
	glBindVertexArray(0);
}

void OGLvPlanet::InitShared(ShaderMgr *shaderMgr, const std::string &texturePath)
{
	if (s_sharedInitialized) return;
	s_sharedInitialized = true;
	s_shaderMgr = shaderMgr;

	s_planetShader = shaderMgr->LoadProgram("planet", "planet.vert", "planet.frag");
	CreateSphere(32, 16, s_sphereVAO, s_sphereVBO, s_sphereEBO, s_sphereIndexCount);

	s_texPlanetShader = shaderMgr->LoadProgram("texplanet", "texplanet.vert", "texplanet.frag");
	CreateTexturedSphere(64, 32, s_texSphereVAO, s_texSphereVBO, s_texSphereEBO, s_texSphereIndexCount);

	InitRingsShared(shaderMgr, texturePath);
}

void OGLvPlanet::ReleaseShared()
{
	if (s_sphereVAO) { glDeleteVertexArrays(1, &s_sphereVAO); s_sphereVAO = 0; }
	if (s_sphereVBO) { glDeleteBuffers(1, &s_sphereVBO); s_sphereVBO = 0; }
	if (s_sphereEBO) { glDeleteBuffers(1, &s_sphereEBO); s_sphereEBO = 0; }
	if (s_texSphereVAO) { glDeleteVertexArrays(1, &s_texSphereVAO); s_texSphereVAO = 0; }
	if (s_texSphereVBO) { glDeleteBuffers(1, &s_texSphereVBO); s_texSphereVBO = 0; }
	if (s_texSphereEBO) { glDeleteBuffers(1, &s_texSphereEBO); s_texSphereEBO = 0; }
	if (s_ringVAO) { glDeleteVertexArrays(1, &s_ringVAO); s_ringVAO = 0; }
	if (s_ringVBO) { glDeleteBuffers(1, &s_ringVBO); s_ringVBO = 0; }
	if (s_ringEBO) { glDeleteBuffers(1, &s_ringEBO); s_ringEBO = 0; }
	delete s_ringTexture; s_ringTexture = nullptr;
	s_ringsInitialized = false;
	s_sharedInitialized = false;
}

void OGLvPlanet::InitRingsShared(ShaderMgr *shaderMgr, const std::string &texturePath)
{
	if (s_ringsInitialized) return;
	s_ringsInitialized = true;

	s_ringShader = shaderMgr->LoadProgram("ring", "ring.vert", "ring.frag");

	const char *ringTexNames[] = {
		"Saturn_ring_4096.dds", "Saturn_ring_2048.dds", "Saturn_ring_8192.dds", nullptr
	};
	for (int i = 0; ringTexNames[i] && !s_ringTexture; i++) {
		std::string path = texturePath + ringTexNames[i];
		if (FileExists(path.c_str()))
			s_ringTexture = OGLTexture::LoadDDS(path.c_str());
	}

	const int nsect = 72;
	struct RingVtx { float x, y, z, u, v; };
	std::vector<RingVtx> verts((nsect + 1) * 2);
	std::vector<unsigned int> indices;
	for (int i = 0; i <= nsect; i++) {
		float angle = (float)i / nsect * 2.0f * M_PI;
		float ca = cosf(angle), sa = sinf(angle);
		verts[i * 2]     = {ca, 0, sa, 0.0f, 0.5f};
		verts[i * 2 + 1] = {ca, 0, sa, 1.0f, 0.5f};
	}
	for (int i = 0; i < nsect; i++) {
		int base = i * 2;
		indices.push_back(base); indices.push_back(base + 1); indices.push_back(base + 2);
		indices.push_back(base + 1); indices.push_back(base + 3); indices.push_back(base + 2);
	}
	s_ringIndexCount = (int)indices.size();

	glGenVertexArrays(1, &s_ringVAO); glGenBuffers(1, &s_ringVBO); glGenBuffers(1, &s_ringEBO);
	glBindVertexArray(s_ringVAO);
	glBindBuffer(GL_ARRAY_BUFFER, s_ringVBO);
	glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(RingVtx), verts.data(), GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_ringEBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(RingVtx), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(RingVtx), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
	glBindVertexArray(0);
}

// --- Instance methods ---

OGLvPlanet::OGLvPlanet(OBJHANDLE hObj, ShaderMgr *shaderMgr)
	: OGLvObject(hObj, shaderMgr), m_texture(nullptr), m_tileMgr(nullptr), m_atmo(nullptr)
{
	// Initialize atmosphere for planets that have one
	if (oapiGetObjectType(hObj) == OBJTP_PLANET)
		m_atmo = new OGLAtmosphere(hObj, shaderMgr);
}

OGLvPlanet::~OGLvPlanet()
{
	delete m_texture;
	delete m_tileMgr;
	delete m_atmo;
}

void OGLvPlanet::InitTiles(const std::string &texturePath)
{
	// Try to find tile archives for this planet
	char name[64];
	oapiGetObjectName(m_hObj, name, 64);

	// Planet data path: typically "Textures/<PlanetName>"
	std::string planetPath = texturePath + name;

	// Check if Archive directory exists
	std::string archivePath = planetPath + "/Archive";
	struct stat st;
	if (stat(archivePath.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
		m_tileMgr = new OGLTileMgr(m_hObj, m_shaderMgr);
		if (!m_tileMgr->Init(planetPath)) {
			delete m_tileMgr;
			m_tileMgr = nullptr;
		}
	}
}

void OGLvPlanet::LoadTexture(const std::string &texturePath)
{
	char name[64];
	oapiGetObjectName(m_hObj, name, 64);

	const char *extensions[] = { ".tex", ".dds", ".bmp", nullptr };
	for (int i = 0; extensions[i]; i++) {
		std::string tryPath = texturePath + name + extensions[i];
		if (FileExists(tryPath.c_str())) {
			m_texture = OGLTexture::LoadTexture(tryPath.c_str());
			if (m_texture) {
				fprintf(stderr, "[OGLvPlanet] Loaded texture '%s' for %s\n", tryPath.c_str(), name);
				return;
			}
		}
	}
	fprintf(stderr, "[OGLvPlanet] No texture found for '%s'\n", name);
}

void OGLvPlanet::Render(const float *vp, const VECTOR3 &camPos, const VECTOR3 &sunPos)
{
	if (!s_sharedInitialized) return;

	// (debug logging removed)

	// Use LOD tile rendering if available
	if (m_tileMgr) {
		double planetRadius = oapiGetSize(m_hObj);
		MATRIX3 planetRot;
		oapiGetRotationMatrix(m_hObj, &planetRot);
		m_tileMgr->Render(vp, camPos, sunPos, planetRadius, planetRot);
		// Render atmospheric haze after surface
		if (m_atmo && m_atmo->HasAtmosphere()) {
			VECTOR3 ppos;
			oapiGetGlobalPos(m_hObj, &ppos);
			m_atmo->Render(vp, camPos, sunPos, planetRadius, ppos);
		}
		RenderRings(vp, camPos, sunPos);
		return;
	}

	// Fallback: single textured sphere
	VECTOR3 pos;
	oapiGetGlobalPos(m_hObj, &pos);

	double rx = pos.x - camPos.x;
	double ry = pos.y - camPos.y;
	double rz = pos.z - camPos.z;
	double dist = sqrt(rx * rx + ry * ry + rz * rz);
	double size = oapiGetSize(m_hObj);
	if (dist < size * 0.5) return;

	// Sun direction at this object
	double sdx = sunPos.x - pos.x, sdy = sunPos.y - pos.y, sdz = sunPos.z - pos.z;
	double sdist = sqrt(sdx * sdx + sdy * sdy + sdz * sdz);
	if (sdist > 0) { sdx /= sdist; sdy /= sdist; sdz /= sdist; }
	float sunDir[3] = {(float)sdx, (float)sdy, (float)sdz};

	// Distance normalization
	double normDist = 10.0;
	double scale = normDist / dist;
	float nrx = (float)(rx * scale), nry = (float)(ry * scale), nrz = (float)(rz * scale);
	float ns = (float)(size * scale);

	float model[16] = {
		ns, 0,  0,  0,
		0,  ns, 0,  0,
		0,  0,  ns, 0,
		nrx, nry, nrz, 1
	};

	if (m_texture && m_texture->texId && s_texPlanetShader && s_texSphereVAO) {
		glUseProgram(s_texPlanetShader);
		glUniformMatrix4fv(s_shaderMgr->GetUniformLoc(s_texPlanetShader, "uViewProj"), 1, GL_FALSE, vp);
		glUniformMatrix4fv(s_shaderMgr->GetUniformLoc(s_texPlanetShader, "uModel"), 1, GL_FALSE, model);
		glUniform3fv(s_shaderMgr->GetUniformLoc(s_texPlanetShader, "uSunDir"), 1, sunDir);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m_texture->texId);
		glUniform1i(s_shaderMgr->GetUniformLoc(s_texPlanetShader, "uTexture"), 0);
		glBindVertexArray(s_texSphereVAO);
		glDrawElements(GL_TRIANGLES, s_texSphereIndexCount, GL_UNSIGNED_INT, 0);
	} else {
		int type = oapiGetObjectType(m_hObj);
		float color[3];
		if (type == OBJTP_STAR) {
			color[0] = 1.0f; color[1] = 0.95f; color[2] = 0.8f;
		} else {
			char name[64];
			oapiGetObjectName(m_hObj, name, 64);
			if (strcmp(name, "Earth") == 0) { color[0] = 0.3f; color[1] = 0.5f; color[2] = 1.0f; }
			else if (strcmp(name, "Moon") == 0) { color[0] = 0.7f; color[1] = 0.7f; color[2] = 0.7f; }
			else if (strcmp(name, "Mars") == 0) { color[0] = 0.8f; color[1] = 0.3f; color[2] = 0.1f; }
			else if (strcmp(name, "Venus") == 0) { color[0] = 0.9f; color[1] = 0.8f; color[2] = 0.6f; }
			else if (strcmp(name, "Jupiter") == 0) { color[0] = 0.8f; color[1] = 0.7f; color[2] = 0.5f; }
			else if (strcmp(name, "Saturn") == 0) { color[0] = 0.9f; color[1] = 0.8f; color[2] = 0.6f; }
			else { color[0] = 0.6f; color[1] = 0.6f; color[2] = 0.6f; }
		}
		glUseProgram(s_planetShader);
		glUniformMatrix4fv(s_shaderMgr->GetUniformLoc(s_planetShader, "uViewProj"), 1, GL_FALSE, vp);
		glUniformMatrix4fv(s_shaderMgr->GetUniformLoc(s_planetShader, "uModel"), 1, GL_FALSE, model);
		glUniform3fv(s_shaderMgr->GetUniformLoc(s_planetShader, "uSunDir"), 1, sunDir);
		glUniform3fv(s_shaderMgr->GetUniformLoc(s_planetShader, "uColor"), 1, color);
		glBindVertexArray(s_sphereVAO);
		glDrawElements(GL_TRIANGLES, s_sphereIndexCount, GL_UNSIGNED_INT, 0);
	}
	glBindVertexArray(0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);

	// Render atmospheric haze
	if (m_atmo && m_atmo->HasAtmosphere()) {
		VECTOR3 ppos;
		oapiGetGlobalPos(m_hObj, &ppos);
		m_atmo->Render(vp, camPos, sunPos, oapiGetSize(m_hObj), ppos);
	}

	// Render rings if applicable
	RenderRings(vp, camPos, sunPos);
}

void OGLvPlanet::RenderRings(const float *vp, const VECTOR3 &camPos, const VECTOR3 &sunPos)
{
	if (!s_ringShader || !s_ringVAO || !s_ringTexture) return;

	const void *pHasRings = oapiGetObjectParam(m_hObj, OBJPRM_PLANET_HASRINGS);
	if (!pHasRings || !*(const bool*)pHasRings) return;

	const void *pMinRad = oapiGetObjectParam(m_hObj, OBJPRM_PLANET_RINGMINRAD);
	const void *pMaxRad = oapiGetObjectParam(m_hObj, OBJPRM_PLANET_RINGMAXRAD);
	if (!pMinRad || !pMaxRad) return;
	double minRad = *(const double*)pMinRad;
	double maxRad = *(const double*)pMaxRad;
	double planetSize = oapiGetSize(m_hObj);

	VECTOR3 pos;
	oapiGetGlobalPos(m_hObj, &pos);
	double rx = pos.x - camPos.x, ry = pos.y - camPos.y, rz = pos.z - camPos.z;
	double dist = sqrt(rx * rx + ry * ry + rz * rz);
	double scale = 10.0 / dist;

	float outerR = (float)(maxRad * planetSize * scale);
	float innerR = (float)(minRad * planetSize * scale);
	float tx = (float)(rx * scale), ty = (float)(ry * scale), tz = (float)(rz * scale);

	MATRIX3 prot;
	oapiGetRotationMatrix(m_hObj, &prot);

	float model[16] = {
		(float)prot.m11, (float)prot.m21, (float)prot.m31, 0,
		(float)prot.m12, (float)prot.m22, (float)prot.m32, 0,
		(float)prot.m13, (float)prot.m23, (float)prot.m33, 0,
		tx, ty, tz, 1
	};

	glUseProgram(s_ringShader);
	glUniformMatrix4fv(s_shaderMgr->GetUniformLoc(s_ringShader, "uViewProj"), 1, GL_FALSE, vp);
	glUniformMatrix4fv(s_shaderMgr->GetUniformLoc(s_ringShader, "uModel"), 1, GL_FALSE, model);
	glUniform1f(s_shaderMgr->GetUniformLoc(s_ringShader, "uInnerRad"), innerR);
	glUniform1f(s_shaderMgr->GetUniformLoc(s_ringShader, "uOuterRad"), outerR);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, s_ringTexture->texId);
	glUniform1i(s_shaderMgr->GetUniformLoc(s_ringShader, "uTexture"), 0);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_CULL_FACE);
	glBindVertexArray(s_ringVAO);
	glDrawElements(GL_TRIANGLES, s_ringIndexCount, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
	glDisable(GL_BLEND);
	glEnable(GL_CULL_FACE);
	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);
}

} // namespace ogl

#endif // !_WIN32
