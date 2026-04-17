// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// OGLTile - Quad-tree tile system for LOD planetary surface rendering

#ifndef __OGLTILE_H
#define __OGLTILE_H

#ifndef _WIN32
#include "OrbiterAPI.h"
#include <OpenGL/gl3.h>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <cstdint>

class ZTreeMgr;
struct OGLTexture;

namespace ogl {

class ShaderMgr;

// Tile state machine
enum class TileState {
	Invalid,    // not loaded
	InQueue,    // queued for async load
	Loading,    // being loaded by background thread
	Active,     // loaded and ready to render
	ForRender   // selected for rendering this frame
};

// A single spherical patch tile
struct OGLTileData {
	int level;          // LOD level (1 = coarsest)
	int ilat, ilng;     // latitude/longitude indices at this level
	TileState state;

	// Geometry (filled on load)
	GLuint vao, vbo, ebo;
	int indexCount;
	int vertexCount;

	// Texture (loaded from .tree archive)
	GLuint texId;
	bool ownsTexture;

	// Bounding sphere on the unit sphere (centre is a direction, radius the
	// tile's half-diagonal in the same unit system). Call-site multiplies
	// by the planet radius when it needs the world-space magnitude.
	float bsCenterX, bsCenterY, bsCenterZ;
	float bsRadius;

	// Frame-count of the most recent render use — the tile cache evicts the
	// least-recently-rendered entries when the pool grows past its budget.
	std::uint64_t lastUsedFrame;

	// Quad-tree children (nullptr if not subdivided)
	OGLTileData *child[4];
	OGLTileData *parent;

	OGLTileData();
	~OGLTileData();
	void ReleaseGL();
	bool HasChildren() const;
};

// Tile load request for the async loader
struct TileLoadRequest {
	OGLTileData *tile;
	OBJHANDLE hPlanet;
	double planetRadius;
};

// Manages the LOD quad-tree for one planet
class OGLTileMgr {
public:
	OGLTileMgr(OBJHANDLE hPlanet, ShaderMgr *shaderMgr);
	~OGLTileMgr();

	// Initialize: open .tree archives, create root tiles
	bool Init(const std::string &planetPath);

	// Process LOD for current camera, queue loads, render visible tiles
	void Render(const float *vp, const VECTOR3 &camPos, const VECTOR3 &sunPos,
	            double planetRadius, const MATRIX3 &planetRot);

	// Process completed tile loads on the main thread (upload textures to GL)
	void ProcessLoadQueue();

	// Release all resources
	void Release();

private:
	OBJHANDLE m_hPlanet;
	ShaderMgr *m_shaderMgr;
	GLuint m_surfShader;

	// ZTreeMgr instances for tile data
	ZTreeMgr *m_treeSurf;   // surface textures
	ZTreeMgr *m_treeMask;   // night lights / specular mask
	ZTreeMgr *m_treeElev;   // elevation data

	// Root tiles (level 1: 2 longitude hemispheres)
	std::vector<OGLTileData*> m_rootTiles;

	// All allocated tiles (for cleanup)
	std::vector<OGLTileData*> m_allTiles;

	// Async tile loader
	std::thread m_loaderThread;
	std::mutex m_loadMutex;
	std::vector<TileLoadRequest> m_loadRequests;
	std::vector<OGLTileData*> m_loadComplete;
	std::atomic<bool> m_loaderRunning;

	// Temp buffers for tile data loaded by background thread
	struct LoadedTileData {
		OGLTileData *tile;
		std::vector<uint8_t> texData;
		int texWidth, texHeight;
		std::vector<float> vertices;
		std::vector<unsigned int> indices;
		float bsCx, bsCy, bsCz, bsRad;

		// M7.b — elevation data parsed from the ZTree ELEV layer.
		// `elev` holds `elevW * elevH` samples in metres (already scaled +
		// offset per the ELEVFILEHEADER); empty when the elev layer is
		// missing or the tile's patch is flat. The BuildSpherePatch path
		// bilinearly samples this grid to Z-offset each vertex.
		std::vector<float> elev;
		int elevW, elevH;
		int elevPadX, elevPadY;
	};
	std::mutex m_completeMutex;
	std::vector<LoadedTileData> m_completedLoads;

	void LoaderThreadFunc();

	// Frame counter stamped on every touched tile (see OGLTileData::lastUsedFrame).
	std::uint64_t m_frame;

	// Traversal context captured once per Render() so ProcessNode doesn't
	// have to re-derive horizon and frustum state for every node.
	struct TraversalCtx {
		VECTOR3 relCam;         // camera position in planet-local frame [m]
		double  camDist;        // |relCam|
		double  planetRadius;
		double  horizonCos;     // cos(angle_to_horizon): planetRadius / camDist
		double  tanAp;          // tan(camera aperture / 2)
		// Frustum planes in the planet-local distance-normalised frame. Each
		// plane is (nx, ny, nz, d) with the convention dot(n, p) + d > 0
		// inside.
		float   frustum[6][4];
		double  normScale;      // normDist / camDist (matches surface pass)
	};

	// LOD traversal.
	void ProcessNode(OGLTileData *tile, int lvl, int ilat, int ilng,
	                 const TraversalCtx &ctx,
	                 std::vector<OGLTileData*> &renderList);

	// Cheap culling helpers — both work in the unit-sphere frame of the
	// bounding sphere; the call site scales by planetRadius where needed.
	bool IsBelowHorizon(const OGLTileData *tile, const TraversalCtx &ctx) const;
	bool IsOutsideFrustum(const OGLTileData *tile, const TraversalCtx &ctx) const;

	// Evict cold tiles when the live pool exceeds kTileCacheBudget. Called
	// opportunistically at the end of Render().
	void EvictColdTiles();
	static constexpr std::size_t kTileCacheBudget = 512;

	// Create a tile and its geometry for a given level/lat/lng
	OGLTileData *CreateTile(int level, int ilat, int ilng, double planetRadius);

	// Build sphere patch geometry for a tile. When `elev` is non-empty the
	// vertex radii are displaced by (1 + elev/planetRadius); otherwise a
	// plain unit-sphere patch is emitted.
	void BuildSpherePatch(int level, int ilat, int ilng, double planetRadius,
	                      const std::vector<float> &elev,
	                      int elevW, int elevH, int elevPadX, int elevPadY,
	                      std::vector<float> &vertices, std::vector<unsigned int> &indices,
	                      float &bsCx, float &bsCy, float &bsCz, float &bsRad);

	// Parse an ELEVFILEHEADER block + raw sample stream into `outSamples`.
	// Returns true when valid data was produced (flat tiles return true with
	// an empty vector). `outW` / `outH` carry the grid dimensions.
	bool ParseElevationBlock(const uint8_t *data, size_t size,
	                         std::vector<float> &outSamples,
	                         int &outW, int &outH, int &outPadX, int &outPadY);

	// Upload geometry to GL on the main thread
	void UploadTileGL(OGLTileData *tile, const std::vector<float> &vertices,
	                  const std::vector<unsigned int> &indices);

	// Upload DDS texture data to GL
	void UploadTileTexture(OGLTileData *tile, const std::vector<uint8_t> &data,
	                       int width, int height);
};

} // namespace ogl

#endif // !_WIN32
#endif // __OGLTILE_H
