// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// OGLvVessel - Vessel/spacecraft rendering (meshes, exhaust plumes)

#ifndef __OGLVVESSEL_H
#define __OGLVVESSEL_H

#ifndef _WIN32
#include "OGLvObject.h"
#include <OpenGL/gl3.h>
#include <cstdint>
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

	// Set PBR material uniforms for a mesh group
	static void SetMaterialUniforms(GLuint shader, MESHHANDLE hMesh, DWORD grpIdx);

	// --- Shared resources ---
	static GLuint s_vesselShader;     // legacy shader (fallback)
	static GLuint s_pbrShader;        // PBR Cook-Torrance shader
	static GLuint s_exhaustShader;
	static GLuint s_exhaustVAO, s_exhaustVBO, s_exhaustEBO;
	static GLuint s_materialUBO;      // bound to UBO::Material for both shaders
	static OGLTexture *s_exhaustTexture;
	static bool s_sharedInitialized;
	static ShaderMgr *s_shaderMgr;

	// GPU mesh cache lives in ogl::MeshRegistry now; see OGLMeshRegistry.h.
	// Fallback mesh cache maps vessel class name to MESHHANDLE, used when
	// VESSEL::GetMeshTemplate() returns null on macOS.
	static std::map<std::string, MESHHANDLE> s_fallbackMeshes;
};

} // namespace ogl

#endif // !_WIN32
#endif // __OGLVVESSEL_H
