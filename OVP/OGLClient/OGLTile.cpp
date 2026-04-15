// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// OGLTile - Quad-tree tile system implementation

#ifndef _WIN32

#include "OGLTile.h"
#include "OGLShaderMgr.h"
#include "OGLTexture.h"
#include "ZTreeMgr.h"
#include <cstdio>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <chrono>

namespace ogl {

// ============================================================================
// OGLTileData
// ============================================================================

OGLTileData::OGLTileData()
	: level(0), ilat(0), ilng(0), state(TileState::Invalid),
	  vao(0), vbo(0), ebo(0), indexCount(0), vertexCount(0),
	  texId(0), ownsTexture(false),
	  bsCenterX(0), bsCenterY(0), bsCenterZ(0), bsRadius(0),
	  parent(nullptr)
{
	child[0] = child[1] = child[2] = child[3] = nullptr;
}

OGLTileData::~OGLTileData()
{
	ReleaseGL();
}

void OGLTileData::ReleaseGL()
{
	if (vao) { glDeleteVertexArrays(1, &vao); vao = 0; }
	if (vbo) { glDeleteBuffers(1, &vbo); vbo = 0; }
	if (ebo) { glDeleteBuffers(1, &ebo); ebo = 0; }
	if (ownsTexture && texId) { glDeleteTextures(1, &texId); texId = 0; }
	indexCount = 0;
}

bool OGLTileData::HasChildren() const
{
	return child[0] || child[1] || child[2] || child[3];
}

// ============================================================================
// OGLTileMgr
// ============================================================================

OGLTileMgr::OGLTileMgr(OBJHANDLE hPlanet, ShaderMgr *shaderMgr)
	: m_hPlanet(hPlanet), m_shaderMgr(shaderMgr), m_surfShader(0),
	  m_treeSurf(nullptr), m_treeMask(nullptr), m_treeElev(nullptr),
	  m_loaderRunning(false)
{
}

OGLTileMgr::~OGLTileMgr()
{
	Release();
}

bool OGLTileMgr::Init(const std::string &planetPath)
{
	m_surfShader = m_shaderMgr->LoadProgram("surfacetile", "surfacetile.vert", "surfacetile.frag");

	// Open tile archives
	m_treeSurf = ZTreeMgr::CreateFromFile(planetPath.c_str(), ZTreeMgr::LAYER_SURF);
	m_treeMask = ZTreeMgr::CreateFromFile(planetPath.c_str(), ZTreeMgr::LAYER_MASK);
	m_treeElev = ZTreeMgr::CreateFromFile(planetPath.c_str(), ZTreeMgr::LAYER_ELEV);

	if (!m_treeSurf) {
		char name[64];
		oapiGetObjectName(m_hPlanet, name, 64);
		fprintf(stderr, "[OGLTileMgr] No surface archive for '%s' at '%s'\n", name, planetPath.c_str());
		return false;
	}

	char name[64];
	oapiGetObjectName(m_hPlanet, name, 64);
	fprintf(stderr, "[OGLTileMgr] Opened tile archives for '%s'\n", name);

	// Start loader thread
	m_loaderRunning = true;
	m_loaderThread = std::thread(&OGLTileMgr::LoaderThreadFunc, this);

	return true;
}

void OGLTileMgr::Release()
{
	// Stop loader thread
	m_loaderRunning = false;
	if (m_loaderThread.joinable())
		m_loaderThread.join();

	// Release all tiles
	for (auto *t : m_allTiles) delete t;
	m_allTiles.clear();
	m_rootTiles.clear();

	delete m_treeSurf; m_treeSurf = nullptr;
	delete m_treeMask; m_treeMask = nullptr;
	delete m_treeElev; m_treeElev = nullptr;
}

void OGLTileMgr::LoaderThreadFunc()
{
	while (m_loaderRunning) {
		TileLoadRequest req;
		bool hasWork = false;

		{
			std::lock_guard<std::mutex> lock(m_loadMutex);
			if (!m_loadRequests.empty()) {
				req = m_loadRequests.back();
				m_loadRequests.pop_back();
				hasWork = true;
			}
		}

		if (!hasWork) {
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
			continue;
		}

		OGLTileData *tile = req.tile;
		LoadedTileData loaded;
		loaded.tile = tile;
		loaded.texWidth = 0;
		loaded.texHeight = 0;

		// Load texture data from ZTree archive
		if (m_treeSurf) {
			BYTE *data = nullptr;
			DWORD size = m_treeSurf->ReadData(tile->level, tile->ilat, tile->ilng, &data);
			if (data && size > 0) {
				loaded.texData.assign(data, data + size);
				m_treeSurf->ReleaseData(data);
			}
		}

		// Build sphere patch geometry
		BuildSpherePatch(tile->level, tile->ilat, tile->ilng, req.planetRadius,
		                 loaded.vertices, loaded.indices,
		                 loaded.bsCx, loaded.bsCy, loaded.bsCz, loaded.bsRad);

		// Queue for main thread upload
		{
			std::lock_guard<std::mutex> lock(m_completeMutex);
			m_completedLoads.push_back(std::move(loaded));
		}
	}
}

void OGLTileMgr::ProcessLoadQueue()
{
	std::vector<LoadedTileData> completed;
	{
		std::lock_guard<std::mutex> lock(m_completeMutex);
		completed.swap(m_completedLoads);
	}

	for (auto &ld : completed) {
		OGLTileData *tile = ld.tile;

		// Upload geometry
		if (!ld.vertices.empty() && !ld.indices.empty())
			UploadTileGL(tile, ld.vertices, ld.indices);

		// Upload texture (DDS data from archive)
		if (!ld.texData.empty()) {
			OGLTexture *tex = OGLTexture::LoadDDSFromMemory(ld.texData.data(), ld.texData.size(), "tile");
			if (tex) {
				tile->texId = tex->texId;
				tile->ownsTexture = true;
				tex->texId = 0; // prevent destructor from deleting
				delete tex;
			}
		}

		tile->bsCenterX = ld.bsCx;
		tile->bsCenterY = ld.bsCy;
		tile->bsCenterZ = ld.bsCz;
		tile->bsRadius = ld.bsRad;
		tile->state = TileState::Active;
	}
}

void OGLTileMgr::BuildSpherePatch(int level, int ilat, int ilng, double planetRadius,
                                    std::vector<float> &vertices, std::vector<unsigned int> &indices,
                                    float &bsCx, float &bsCy, float &bsCz, float &bsRad)
{
	// Grid resolution for this tile
	int res = 16; // vertices per edge

	int nlat = 1 << level;
	int nlng = 2 << level;

	double latMin = M_PI * 0.5 * (double)(nlat - 1 - ilat) / (double)nlat - M_PI * 0.5;
	double latMax = M_PI * 0.5 * (double)(nlat - ilat) / (double)nlat - M_PI * 0.5;
	double lngMin = 2.0 * M_PI * (double)ilng / (double)nlng;
	double lngMax = 2.0 * M_PI * (double)(ilng + 1) / (double)nlng;

	// Vertex layout: x, y, z, nx, ny, nz, u, v (8 floats per vertex)
	int nVtx = (res + 1) * (res + 1);
	vertices.resize(nVtx * 8);

	double cx = 0, cy = 0, cz = 0;

	for (int i = 0; i <= res; i++) {
		double lat = latMin + (latMax - latMin) * i / res;
		double v = (double)i / res;
		for (int j = 0; j <= res; j++) {
			double lng = lngMin + (lngMax - lngMin) * j / res;
			double u = (double)j / res;

			double cosLat = cos(lat), sinLat = sin(lat);
			double cosLng = cos(lng), sinLng = sin(lng);

			float px = (float)(cosLat * cosLng);
			float py = (float)(sinLat);
			float pz = (float)(cosLat * sinLng);

			int idx = (i * (res + 1) + j) * 8;
			vertices[idx + 0] = px;         // x
			vertices[idx + 1] = py;         // y
			vertices[idx + 2] = pz;         // z
			vertices[idx + 3] = px;         // nx (unit sphere: normal = position)
			vertices[idx + 4] = py;         // ny
			vertices[idx + 5] = pz;         // nz
			vertices[idx + 6] = (float)u;   // u
			vertices[idx + 7] = (float)v;   // v

			cx += px; cy += py; cz += pz;
		}
	}

	// Bounding sphere center
	cx /= nVtx; cy /= nVtx; cz /= nVtx;
	bsCx = (float)cx; bsCy = (float)cy; bsCz = (float)cz;

	// Bounding sphere radius
	float maxDistSq = 0;
	for (int i = 0; i < nVtx; i++) {
		float dx = vertices[i * 8 + 0] - bsCx;
		float dy = vertices[i * 8 + 1] - bsCy;
		float dz = vertices[i * 8 + 2] - bsCz;
		float dSq = dx * dx + dy * dy + dz * dz;
		if (dSq > maxDistSq) maxDistSq = dSq;
	}
	bsRad = sqrtf(maxDistSq);

	// Build index buffer (triangle strip → triangles)
	indices.clear();
	for (int i = 0; i < res; i++) {
		for (int j = 0; j < res; j++) {
			int a = i * (res + 1) + j;
			int b = a + res + 1;
			indices.push_back(a);
			indices.push_back(b);
			indices.push_back(a + 1);
			indices.push_back(a + 1);
			indices.push_back(b);
			indices.push_back(b + 1);
		}
	}
}

void OGLTileMgr::UploadTileGL(OGLTileData *tile, const std::vector<float> &vertices,
                                const std::vector<unsigned int> &indices)
{
	tile->ReleaseGL(); // clean up any previous data

	glGenVertexArrays(1, &tile->vao);
	glGenBuffers(1, &tile->vbo);
	glGenBuffers(1, &tile->ebo);
	glBindVertexArray(tile->vao);

	glBindBuffer(GL_ARRAY_BUFFER, tile->vbo);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, tile->ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

	// layout(location=0) vec3 position
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	// layout(location=1) vec3 normal
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
	// layout(location=2) vec2 uv
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
	glEnableVertexAttribArray(2);

	glBindVertexArray(0);

	tile->indexCount = (int)indices.size();
	tile->vertexCount = (int)(vertices.size() / 8);
}

OGLTileData *OGLTileMgr::CreateTile(int level, int ilat, int ilng, double planetRadius)
{
	OGLTileData *tile = new OGLTileData();
	tile->level = level;
	tile->ilat = ilat;
	tile->ilng = ilng;
	tile->state = TileState::InQueue;
	m_allTiles.push_back(tile);

	// Queue for async loading
	TileLoadRequest req;
	req.tile = tile;
	req.hPlanet = m_hPlanet;
	req.planetRadius = planetRadius;

	{
		std::lock_guard<std::mutex> lock(m_loadMutex);
		m_loadRequests.push_back(req);
	}

	return tile;
}

void OGLTileMgr::ProcessNode(OGLTileData *tile, int lvl, int ilat, int ilng,
                              const VECTOR3 &camPos, double planetRadius,
                              double tanAp, std::vector<OGLTileData*> &renderList)
{
	if (!tile || tile->state == TileState::Invalid) return;
	if (tile->state == TileState::InQueue || tile->state == TileState::Loading) {
		// Tile is loading — render parent if available
		return;
	}

	// Simple LOD: check angular size of tile vs screen
	// Tile center in planet-local coordinates (unit sphere)
	float cx = tile->bsCenterX, cy = tile->bsCenterY, cz = tile->bsCenterZ;

	// Transform to global (approximate — just multiply by planetRadius)
	double tcx = cx * planetRadius;
	double tcy = cy * planetRadius;
	double tcz = cz * planetRadius;

	// Distance from camera to tile center
	double dx = tcx - camPos.x, dy = tcy - camPos.y, dz = tcz - camPos.z;
	double dist = sqrt(dx * dx + dy * dy + dz * dz);

	// Angular size of tile as seen from camera
	double angularSize = tile->bsRadius * planetRadius / dist;

	// LOD threshold: subdivide if tile subtends more than ~0.5 radians on screen
	double threshold = 0.3 * tanAp;
	bool shouldSubdivide = (angularSize > threshold) && (lvl < 14);

	// Check if tile data is available in the archive
	bool hasData = false;
	if (m_treeSurf) {
		DWORD idx = m_treeSurf->Idx(lvl, ilat, ilng);
		hasData = (idx != (DWORD)-1);
	}

	if (shouldSubdivide && hasData) {
		// Try to subdivide
		int nlat_child = ilat * 2;
		int nlng_child = ilng * 2;

		for (int c = 0; c < 4; c++) {
			int clat = nlat_child + (c >> 1);
			int clng = nlng_child + (c & 1);

			if (!tile->child[c]) {
				// Check if child data exists in archive
				if (m_treeSurf) {
					DWORD cidx = m_treeSurf->Idx(lvl + 1, clat, clng);
					if (cidx != (DWORD)-1)
						tile->child[c] = CreateTile(lvl + 1, clat, clng, planetRadius);
				}
			}

			if (tile->child[c] && tile->child[c]->state == TileState::Active)
				ProcessNode(tile->child[c], lvl + 1, clat, clng, camPos, planetRadius, tanAp, renderList);
			else
				renderList.push_back(tile); // fallback to parent
		}
	} else {
		renderList.push_back(tile);
	}
}

void OGLTileMgr::Render(const float *vp, const VECTOR3 &camPos, const VECTOR3 &sunPos,
                          double planetRadius, const MATRIX3 &planetRot)
{
	if (!m_surfShader) return;

	// Process completed async loads
	ProcessLoadQueue();

	// Ensure root tiles exist
	if (m_rootTiles.empty()) {
		// Create level-1 root tiles (2 hemispheres: ilng=0,1)
		for (int ilng = 0; ilng < 2; ilng++) {
			OGLTileData *root = CreateTile(1, 0, ilng, planetRadius);
			m_rootTiles.push_back(root);
		}
	}

	// Camera position relative to planet center (in planet coordinates)
	VECTOR3 planetPos;
	oapiGetGlobalPos(m_hPlanet, &planetPos);
	VECTOR3 relCam = { camPos.x - planetPos.x, camPos.y - planetPos.y, camPos.z - planetPos.z };

	double fov = oapiCameraAperture() * 2.0;
	if (fov <= 0) fov = 50.0 * M_PI / 180.0;
	double tanAp = tan(fov * 0.5);

	// Build render list via LOD traversal
	std::vector<OGLTileData*> renderList;
	for (auto *root : m_rootTiles) {
		if (root->state == TileState::Active)
			ProcessNode(root, root->level, root->ilat, root->ilng, relCam, planetRadius, tanAp, renderList);
	}

	if (renderList.empty()) return;

	// Remove duplicates
	std::sort(renderList.begin(), renderList.end());
	renderList.erase(std::unique(renderList.begin(), renderList.end()), renderList.end());

	// Distance normalization (same as OGLvPlanet)
	double rx = planetPos.x - camPos.x, ry = planetPos.y - camPos.y, rz = planetPos.z - camPos.z;
	double dist = sqrt(rx * rx + ry * ry + rz * rz);
	double normDist = 10.0;
	double scale = normDist / dist;
	float nrx = (float)(rx * scale), nry = (float)(ry * scale), nrz = (float)(rz * scale);
	float ns = (float)(planetRadius * scale);

	// Sun direction
	double sdx = sunPos.x - planetPos.x, sdy = sunPos.y - planetPos.y, sdz = sunPos.z - planetPos.z;
	double sdist = sqrt(sdx * sdx + sdy * sdy + sdz * sdz);
	if (sdist > 0) { sdx /= sdist; sdy /= sdist; sdz /= sdist; }
	float sunDir[3] = { (float)sdx, (float)sdy, (float)sdz };

	// Model matrix: planet rotation + translation + scale
	float model[16] = {
		(float)(planetRot.m11 * ns), (float)(planetRot.m21 * ns), (float)(planetRot.m31 * ns), 0,
		(float)(planetRot.m12 * ns), (float)(planetRot.m22 * ns), (float)(planetRot.m32 * ns), 0,
		(float)(planetRot.m13 * ns), (float)(planetRot.m23 * ns), (float)(planetRot.m33 * ns), 0,
		nrx, nry, nrz, 1
	};

	glUseProgram(m_surfShader);
	glUniformMatrix4fv(m_shaderMgr->GetUniformLoc(m_surfShader, "uViewProj"), 1, GL_FALSE, vp);
	glUniformMatrix4fv(m_shaderMgr->GetUniformLoc(m_surfShader, "uModel"), 1, GL_FALSE, model);
	glUniform3fv(m_shaderMgr->GetUniformLoc(m_surfShader, "uSunDir"), 1, sunDir);

	for (auto *tile : renderList) {
		if (!tile->vao || tile->indexCount == 0) continue;

		if (tile->texId) {
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, tile->texId);
			glUniform1i(m_shaderMgr->GetUniformLoc(m_surfShader, "uTexture"), 0);
			glUniform1i(m_shaderMgr->GetUniformLoc(m_surfShader, "uHasTexture"), 1);
		} else {
			glUniform1i(m_shaderMgr->GetUniformLoc(m_surfShader, "uHasTexture"), 0);
		}

		glBindVertexArray(tile->vao);
		glDrawElements(GL_TRIANGLES, tile->indexCount, GL_UNSIGNED_INT, 0);
	}

	glBindVertexArray(0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);
}

} // namespace ogl

#endif // !_WIN32
