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

	// Bounding sphere for frustum culling
	float bsCenterX, bsCenterY, bsCenterZ;
	float bsRadius;

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
	};
	std::mutex m_completeMutex;
	std::vector<LoadedTileData> m_completedLoads;

	void LoaderThreadFunc();

	// LOD traversal
	void ProcessNode(OGLTileData *tile, int lvl, int ilat, int ilng,
	                 const VECTOR3 &camPos, double planetRadius,
	                 double tanAp, std::vector<OGLTileData*> &renderList);

	// Create a tile and its geometry for a given level/lat/lng
	OGLTileData *CreateTile(int level, int ilat, int ilng, double planetRadius);

	// Build sphere patch geometry for a tile
	void BuildSpherePatch(int level, int ilat, int ilng, double planetRadius,
	                      std::vector<float> &vertices, std::vector<unsigned int> &indices,
	                      float &bsCx, float &bsCy, float &bsCz, float &bsRad);

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
