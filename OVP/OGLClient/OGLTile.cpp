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
	  lastUsedFrame(0), parent(nullptr)
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
	  m_loaderRunning(false), m_frame(0)
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
				// Priority dequeue: serve the coarsest-level tiles first so
				// the planet's silhouette becomes visible as quickly as
				// possible, then refine toward the camera. Ties are broken
				// by insertion order (LIFO) which keeps cache coherency for
				// siblings requested in the same frame.
				auto best = m_loadRequests.end();
				int bestLvl = 1 << 30;
				for (auto it = m_loadRequests.begin(); it != m_loadRequests.end(); ++it) {
					if (it->tile && it->tile->level < bestLvl) {
						bestLvl = it->tile->level;
						best = it;
					}
				}
				if (best == m_loadRequests.end())
					best = std::prev(m_loadRequests.end());
				req = *best;
				m_loadRequests.erase(best);
				hasWork = true;
			}
		}

		if (!hasWork) {
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
			continue;
		}

		OGLTileData *tile = req.tile;
		LoadedTileData loaded;
		loaded.tile      = tile;
		loaded.texWidth  = 0;
		loaded.texHeight = 0;
		loaded.elevW     = 0;
		loaded.elevH     = 0;
		loaded.elevPadX  = 0;
		loaded.elevPadY  = 0;

		// Load texture data from ZTree archive
		if (m_treeSurf) {
			BYTE *data = nullptr;
			DWORD size = m_treeSurf->ReadData(tile->level, tile->ilat, tile->ilng, &data);
			if (data && size > 0) {
				loaded.texData.assign(data, data + size);
				m_treeSurf->ReleaseData(data);
			}
		}

		// M7.b — elevation. The ELEV layer typically lags the SURF layer by
		// several levels, so missing data is normal and not an error.
		if (m_treeElev) {
			BYTE *data = nullptr;
			DWORD size = m_treeElev->ReadData(tile->level, tile->ilat, tile->ilng, &data);
			if (data && size > 0) {
				ParseElevationBlock(data, size, loaded.elev,
				                    loaded.elevW, loaded.elevH,
				                    loaded.elevPadX, loaded.elevPadY);
				m_treeElev->ReleaseData(data);
			}
		}

		// Build sphere patch geometry (with elevation displacement when available)
		BuildSpherePatch(tile->level, tile->ilat, tile->ilng, req.planetRadius,
		                 loaded.elev, loaded.elevW, loaded.elevH,
		                 loaded.elevPadX, loaded.elevPadY,
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

bool OGLTileMgr::ParseElevationBlock(const uint8_t *data, size_t size,
                                     std::vector<float> &outSamples,
                                     int &outW, int &outH, int &outPadX, int &outPadY)
{
	outSamples.clear();
	outW = outH = outPadX = outPadY = 0;

#pragma pack(push, 1)
	struct Hdr {
		char     id[4];
		int      hdrsize;
		int      dtype;
		int      xgrd, ygrd;
		int      xpad, ypad;
		double   scale;
		double   offset;
		double   latmin, latmax;
		double   lngmin, lngmax;
		double   emin, emax, emean;
	};
#pragma pack(pop)
	if (size < sizeof(Hdr)) return false;

	Hdr h;
	std::memcpy(&h, data, sizeof(Hdr));
	if (h.id[0] != 'E' || h.id[1] != 'L' || h.id[2] != 'E') return false;
	if (h.hdrsize < (int)sizeof(Hdr)) return false;

	outW    = h.xgrd;
	outH    = h.ygrd;
	outPadX = h.xpad;
	outPadY = h.ypad;

	const size_t nSamples = (size_t)h.xgrd * (size_t)h.ygrd;
	if (nSamples == 0) return true;

	const uint8_t *payload = data + h.hdrsize;
	size_t payloadSize = size - h.hdrsize;

	outSamples.resize(nSamples, (float)h.offset);  // flat baseline
	switch (h.dtype) {
	case 0:  // flat (no data block) — baseline already applied
		break;
	case 8: { // uint8
		if (payloadSize < nSamples) return false;
		for (size_t i = 0; i < nSamples; i++)
			outSamples[i] = float(payload[i] * h.scale + h.offset);
		break;
	}
	case -8: { // int8
		if (payloadSize < nSamples) return false;
		const int8_t *s = (const int8_t *)payload;
		for (size_t i = 0; i < nSamples; i++)
			outSamples[i] = float(s[i] * h.scale + h.offset);
		break;
	}
	case 16: { // uint16
		if (payloadSize < nSamples * 2) return false;
		const uint16_t *s = (const uint16_t *)payload;
		for (size_t i = 0; i < nSamples; i++)
			outSamples[i] = float(s[i] * h.scale + h.offset);
		break;
	}
	case -16: { // int16
		if (payloadSize < nSamples * 2) return false;
		const int16_t *s = (const int16_t *)payload;
		for (size_t i = 0; i < nSamples; i++)
			outSamples[i] = float(s[i] * h.scale + h.offset);
		break;
	}
	default:
		outSamples.clear();
		return false;
	}
	return true;
}

// Bilinearly sample the elevation grid at fractional (i, j) where
// (i, j) ∈ [0..innerW-1] × [0..innerH-1] (the inner grid, excluding padding).
static float SampleElev(const std::vector<float> &elev, int W, int H,
                        int padX, int padY,
                        double fracCol, double fracRow)
{
	const int innerW = W - 2 * padX;
	const int innerH = H - 2 * padY;
	if (innerW <= 0 || innerH <= 0) return 0.0f;

	double x = fracCol * (innerW - 1);
	double y = fracRow * (innerH - 1);
	int x0 = (int)std::floor(x);
	int y0 = (int)std::floor(y);
	int x1 = std::min(x0 + 1, innerW - 1);
	int y1 = std::min(y0 + 1, innerH - 1);
	double fx = x - x0;
	double fy = y - y0;

	auto get = [&](int cx, int cy) {
		return elev[(size_t)(cy + padY) * W + (cx + padX)];
	};
	float a = get(x0, y0);
	float b = get(x1, y0);
	float c = get(x0, y1);
	float d = get(x1, y1);
	float ab = float(a * (1.0 - fx) + b * fx);
	float cd = float(c * (1.0 - fx) + d * fx);
	return float(ab * (1.0 - fy) + cd * fy);
}

void OGLTileMgr::BuildSpherePatch(int level, int ilat, int ilng, double planetRadius,
                                    const std::vector<float> &elev,
                                    int elevW, int elevH, int elevPadX, int elevPadY,
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

	const bool hasElev = !elev.empty() && elevW > 0 && elevH > 0;

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

			// Unit-sphere position + normal.
			float nx = (float)(cosLat * cosLng);
			float ny = (float)(sinLat);
			float nz = (float)(cosLat * sinLng);

			// Elevation displacement: shift the vertex radially outward by
			// (elev / planetRadius) normalised units. Sampling flips the
			// row index so the grid's row 0 corresponds to latMin (south).
			float radialScale = 1.0f;
			if (hasElev) {
				float e = SampleElev(elev, elevW, elevH, elevPadX, elevPadY,
				                     u, 1.0 - v);  // row 0 = latMin
				radialScale = 1.0f + e / (float)planetRadius;
			}

			float px = nx * radialScale;
			float py = ny * radialScale;
			float pz = nz * radialScale;

			int idx = (i * (res + 1) + j) * 8;
			vertices[idx + 0] = px;
			vertices[idx + 1] = py;
			vertices[idx + 2] = pz;
			// Normal — for flat tiles the sphere normal is exactly the
			// position direction. For displaced tiles it diverges; we
			// approximate using the position direction, which the PBR
			// lighting still reads correctly. A proper per-vertex normal
			// from finite differences can land with the optional skirt
			// pass (still pending).
			vertices[idx + 3] = nx;
			vertices[idx + 4] = ny;
			vertices[idx + 5] = nz;
			vertices[idx + 6] = (float)u;
			vertices[idx + 7] = (float)v;

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

bool OGLTileMgr::IsBelowHorizon(const OGLTileData *tile, const TraversalCtx &ctx) const
{
	// A tile is safely below the horizon if its entire bounding sphere sits
	// farther from the camera's look-away half-space than the planet's limb.
	// In planet-local coordinates: project the tile centre onto the camera
	// direction and compare with the horizon cosine.
	double tcx = double(tile->bsCenterX) * ctx.planetRadius;
	double tcy = double(tile->bsCenterY) * ctx.planetRadius;
	double tcz = double(tile->bsCenterZ) * ctx.planetRadius;
	double tc2 = tcx * tcx + tcy * tcy + tcz * tcz;
	if (tc2 < 1e-6) return false;

	// Cosine between (camera → planet centre) and (camera → tile centre).
	double dx = ctx.relCam.x, dy = ctx.relCam.y, dz = ctx.relCam.z;
	double camToTileDot = dx * tcx + dy * tcy + dz * tcz;  // not unit-normalised yet
	double tcLen = std::sqrt(tc2);
	double tileConeCos = camToTileDot / (ctx.camDist * tcLen + 1e-9);

	// Allow a generous margin for the sphere radius — a tile whose centre
	// is just past the horizon but whose geometry peeks over it must still
	// render. Convert the bounding sphere radius into an angular slack.
	double angularSlack = double(tile->bsRadius) * ctx.planetRadius / ctx.camDist;

	// The dot-product's sign is flipped because relCam points *away* from
	// the planet centre while the tile vector points *toward* the tile.
	// We want: tile is below horizon ↔ the tile-to-camera angle exceeds
	// the horizon angle. Equivalently: tileConeCos > -horizonCos + slack.
	return tileConeCos > -ctx.horizonCos + angularSlack + 0.02;
}

bool OGLTileMgr::IsOutsideFrustum(const OGLTileData *tile, const TraversalCtx &ctx) const
{
	// Tile centre and radius expressed in the render-time distance-normalised
	// frame: multiply by planetRadius to get planet-local metres, then by the
	// same `normScale` the surface pass applies to the model matrix. This
	// keeps the geometry in the same coordinate system as the frustum planes
	// extracted from VP.
	const double norm = ctx.planetRadius * ctx.normScale;
	double cx = double(tile->bsCenterX) * norm;
	double cy = double(tile->bsCenterY) * norm;
	double cz = double(tile->bsCenterZ) * norm;
	double r  = double(tile->bsRadius)  * norm;

	// Apply the same translation the model matrix encodes — the tile centre
	// in unit-sphere space is relative to the planet centre, whose rendered
	// position is (nrx, nry, nrz). We decompose ctx.relCam back into that:
	//   renderedPlanetPos = -(relCam * normScale)
	double px = -ctx.relCam.x * ctx.normScale;
	double py = -ctx.relCam.y * ctx.normScale;
	double pz = -ctx.relCam.z * ctx.normScale;

	cx += px; cy += py; cz += pz;

	for (int i = 0; i < 6; i++) {
		const float *p = ctx.frustum[i];
		double d = double(p[0]) * cx + double(p[1]) * cy + double(p[2]) * cz + double(p[3]);
		if (d < -r) return true;  // fully outside this plane
	}
	return false;
}

void OGLTileMgr::ProcessNode(OGLTileData *tile, int lvl, int ilat, int ilng,
                              const TraversalCtx &ctx,
                              std::vector<OGLTileData*> &renderList)
{
	if (!tile || tile->state == TileState::Invalid) return;
	if (tile->state == TileState::InQueue || tile->state == TileState::Loading)
		return;

	// Horizon + frustum culling — both use the bounding sphere which is
	// populated on tile upload. Skip nodes outside the view; their children
	// inherit the parent's verdict so we never recurse into hidden regions.
	if (IsBelowHorizon(tile, ctx)) return;
	if (IsOutsideFrustum(tile, ctx)) return;

	// Angular-size LOD: subdivide while the tile subtends a meaningful
	// fraction of the camera aperture.
	const double tcx = double(tile->bsCenterX) * ctx.planetRadius;
	const double tcy = double(tile->bsCenterY) * ctx.planetRadius;
	const double tcz = double(tile->bsCenterZ) * ctx.planetRadius;
	const double dx  = tcx - ctx.relCam.x;
	const double dy  = tcy - ctx.relCam.y;
	const double dz  = tcz - ctx.relCam.z;
	const double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
	const double angularSize = double(tile->bsRadius) * ctx.planetRadius / (dist + 1e-3);
	const double threshold   = 0.3 * ctx.tanAp;
	const bool   shouldSubdivide = (angularSize > threshold) && (lvl < 14);

	bool hasData = false;
	if (m_treeSurf) {
		DWORD idx = m_treeSurf->Idx(lvl, ilat, ilng);
		hasData = (idx != (DWORD)-1);
	}

	if (shouldSubdivide && hasData) {
		const int nlat_child = ilat * 2;
		const int nlng_child = ilng * 2;
		for (int c = 0; c < 4; c++) {
			int clat = nlat_child + (c >> 1);
			int clng = nlng_child + (c & 1);

			if (!tile->child[c]) {
				if (m_treeSurf) {
					DWORD cidx = m_treeSurf->Idx(lvl + 1, clat, clng);
					if (cidx != (DWORD)-1)
						tile->child[c] = CreateTile(lvl + 1, clat, clng, ctx.planetRadius);
				}
			}

			if (tile->child[c] && tile->child[c]->state == TileState::Active)
				ProcessNode(tile->child[c], lvl + 1, clat, clng, ctx, renderList);
			else
				renderList.push_back(tile); // child missing/loading → use parent
		}
	} else {
		renderList.push_back(tile);
	}
}

void OGLTileMgr::EvictColdTiles()
{
	if (m_allTiles.size() <= kTileCacheBudget) return;

	// Sort by lastUsedFrame (cold tiles first). Roots are pinned so the
	// planet silhouette never evaporates under a long burn away from it.
	std::vector<OGLTileData *> candidates;
	candidates.reserve(m_allTiles.size());
	for (auto *t : m_allTiles) {
		bool isRoot = false;
		for (auto *r : m_rootTiles) if (r == t) { isRoot = true; break; }
		if (isRoot) continue;
		if (t->state != TileState::Active) continue;
		candidates.push_back(t);
	}
	std::sort(candidates.begin(), candidates.end(),
	          [](const OGLTileData *a, const OGLTileData *b) {
		          return a->lastUsedFrame < b->lastUsedFrame;
	          });

	const std::size_t keep = kTileCacheBudget * 3 / 4; // aim under budget
	if (m_allTiles.size() <= keep) return;
	const std::size_t toDrop = m_allTiles.size() - keep;

	std::size_t dropped = 0;
	for (OGLTileData *victim : candidates) {
		if (dropped >= toDrop) break;

		// Detach from parent so the quadtree no longer points at a dead node.
		if (victim->parent) {
			for (int c = 0; c < 4; c++)
				if (victim->parent->child[c] == victim)
					victim->parent->child[c] = nullptr;
		}

		auto it = std::find(m_allTiles.begin(), m_allTiles.end(), victim);
		if (it != m_allTiles.end())
			m_allTiles.erase(it);
		delete victim;
		++dropped;
	}
}

// Extract six frustum planes from a column-major view-projection matrix,
// using the standard Gribb-Hartmann decomposition (row4 ± row_i). Each plane
// is returned as {nx, ny, nz, d} with the inside-positive convention.
static void ExtractFrustumPlanes(const float *vp, float out[6][4])
{
	auto get = [&](int col, int row) { return vp[col * 4 + row]; };

	// row_i is {get(0,i), get(1,i), get(2,i), get(3,i)}.
	float r0[4] = { get(0,0), get(1,0), get(2,0), get(3,0) };
	float r1[4] = { get(0,1), get(1,1), get(2,1), get(3,1) };
	float r2[4] = { get(0,2), get(1,2), get(2,2), get(3,2) };
	float r3[4] = { get(0,3), get(1,3), get(2,3), get(3,3) };

	auto add = [](const float a[4], const float b[4], float o[4]) {
		o[0]=a[0]+b[0]; o[1]=a[1]+b[1]; o[2]=a[2]+b[2]; o[3]=a[3]+b[3];
	};
	auto sub = [](const float a[4], const float b[4], float o[4]) {
		o[0]=a[0]-b[0]; o[1]=a[1]-b[1]; o[2]=a[2]-b[2]; o[3]=a[3]-b[3];
	};

	add(r3, r0, out[0]); // left
	sub(r3, r0, out[1]); // right
	add(r3, r1, out[2]); // bottom
	sub(r3, r1, out[3]); // top
	add(r3, r2, out[4]); // near
	sub(r3, r2, out[5]); // far

	for (int i = 0; i < 6; i++) {
		float nx = out[i][0], ny = out[i][1], nz = out[i][2];
		float len = std::sqrt(nx * nx + ny * ny + nz * nz);
		if (len > 1e-6f) {
			float inv = 1.0f / len;
			out[i][0] *= inv; out[i][1] *= inv;
			out[i][2] *= inv; out[i][3] *= inv;
		}
	}
}

void OGLTileMgr::Render(const float *vp, const VECTOR3 &camPos, const VECTOR3 &sunPos,
                          double planetRadius, const MATRIX3 &planetRot)
{
	if (!m_surfShader) return;

	++m_frame;

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

	// Camera position relative to planet center (in planet coordinates).
	VECTOR3 planetPos;
	oapiGetGlobalPos(m_hPlanet, &planetPos);
	VECTOR3 relCam = { camPos.x - planetPos.x, camPos.y - planetPos.y, camPos.z - planetPos.z };
	double rx = planetPos.x - camPos.x, ry = planetPos.y - camPos.y, rz = planetPos.z - camPos.z;
	double dist = std::sqrt(rx * rx + ry * ry + rz * rz);
	double normDist = 10.0;
	double scale = normDist / dist;
	float nrx = (float)(rx * scale), nry = (float)(ry * scale), nrz = (float)(rz * scale);
	float ns  = (float)(planetRadius * scale);

	double fov = oapiCameraAperture() * 2.0;
	if (fov <= 0) fov = 50.0 * M_PI / 180.0;

	// Build traversal context once per frame.
	TraversalCtx ctx;
	ctx.relCam       = relCam;
	ctx.camDist      = std::max(dist, planetRadius + 1.0);
	ctx.planetRadius = planetRadius;
	ctx.horizonCos   = planetRadius / ctx.camDist;
	ctx.tanAp        = std::tan(fov * 0.5);
	ctx.normScale    = scale;
	ExtractFrustumPlanes(vp, ctx.frustum);

	// Build render list via LOD traversal.
	std::vector<OGLTileData*> renderList;
	for (auto *root : m_rootTiles) {
		if (root->state == TileState::Active)
			ProcessNode(root, root->level, root->ilat, root->ilng, ctx, renderList);
	}

	if (renderList.empty()) {
		EvictColdTiles();
		return;
	}

	// Remove duplicates.
	std::sort(renderList.begin(), renderList.end());
	renderList.erase(std::unique(renderList.begin(), renderList.end()), renderList.end());

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

		tile->lastUsedFrame = m_frame;

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

	EvictColdTiles();
}

} // namespace ogl

#endif // !_WIN32
