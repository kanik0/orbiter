// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// OGLvVessel - Vessel/spacecraft rendering (meshes, exhaust plumes)

#ifndef __OGLVVESSEL_H
#define __OGLVVESSEL_H

#ifndef _WIN32
#include "OGLvObject.h"
#include <OpenGL/gl3.h>
#include <map>
#include <string>
#include <vector>

struct OGLTexture;

namespace ogl {

// Cached OpenGL buffers for a single mesh group
struct CachedMeshGroup {
	GLuint vao;
	GLuint vbo;
	GLuint ebo;
	int indexCount;
};

// Cached OpenGL buffers for an entire mesh (all groups)
struct CachedMesh {
	std::vector<CachedMeshGroup> groups;
	~CachedMesh();
};

class OGLvVessel : public OGLvObject {
public:
	OGLvVessel(OBJHANDLE hObj, ShaderMgr *shaderMgr);
	~OGLvVessel() override;

	// One-time init for shared resources (shaders, exhaust geometry)
	static void InitShared(ShaderMgr *shaderMgr, const std::string &texturePath);
	static void ReleaseShared();

	void Render(const float *vp, const VECTOR3 &camPos, const VECTOR3 &sunPos) override;

private:
	// Get or create cached GPU buffers for a mesh
	static CachedMesh *GetOrCreateMeshCache(MESHHANDLE hMesh);

	// Render exhaust plumes for this vessel
	void RenderExhausts(VESSEL *vessel, const MATRIX3 &vrot,
	                    float tx, float ty, float tz, float scale,
	                    const float *vp, const VECTOR3 &camPos);

	// --- Shared resources ---
	static GLuint s_vesselShader;
	static GLuint s_exhaustShader;
	static GLuint s_exhaustVAO, s_exhaustVBO, s_exhaustEBO;
	static OGLTexture *s_exhaustTexture;
	static bool s_sharedInitialized;
	static ShaderMgr *s_shaderMgr;

	// Mesh cache: maps MESHHANDLE to GPU buffers
	static std::map<uintptr_t, CachedMesh*> s_meshCache;

	// Fallback mesh cache: maps class name to MESHHANDLE
	static std::map<std::string, MESHHANDLE> s_fallbackMeshes;
};

} // namespace ogl

#endif // !_WIN32
#endif // __OGLVVESSEL_H
