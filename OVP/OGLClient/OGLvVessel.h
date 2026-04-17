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

namespace oapi { class GraphicsClient; }

namespace ogl {

class OGLEnvMap;
class OGLShadowMap;

// Cached OpenGL buffers for a single mesh group.
//
// `tbnVbo` carries a second interleaved attribute stream with the vertex
// tangent (vec4: xyz = tangent direction, w = bitangent handedness sign).
// It's kept in a separate VBO so the primary NTVERTEX buffer layout stays
// unchanged, and so we don't pay the upload cost on mesh groups that end up
// rendered with the non-PBR shader.
struct CachedMeshGroup {
	GLuint vao;
	GLuint vbo;
	GLuint tbnVbo;
	GLuint ebo;
	int indexCount;
	bool   hasTangent;
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

	// Inject the scene's IBL cubemap provider. Called by OGLScene once the
	// prefilter chain is ready; vessels bind it during render and switch
	// matHasEnvMap on whenever the pointer is non-null.
	static void SetEnvMap(OGLEnvMap *env) { s_envMap = env; }

	// Inject the GraphicsClient so the VC pass can query VC MFD/HUD surfaces
	// via the virtual GetVCMFDSurface / GetVCHUDSurface methods. Called by
	// OGLClient right after its OGLScene is up.
	static void SetGraphicsClient(oapi::GraphicsClient *gc) { s_gc = gc; }

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
	static GLuint s_shadowShader;     // depth-only pre-pass shader
	static OGLShadowMap *s_shadowMap; // shared 1024x1024 depth RT for self-shadowing
	static OGLTexture *s_exhaustTexture;
	static bool s_sharedInitialized;
	static ShaderMgr *s_shaderMgr;
	static OGLEnvMap *s_envMap;       // IBL environment (nullptr until scene bakes)
	static oapi::GraphicsClient *s_gc; // client handle for VC MFD/HUD surface lookups

	// GPU mesh cache lives in ogl::MeshRegistry now; see OGLMeshRegistry.h.
	// Fallback mesh cache maps vessel class name to MESHHANDLE, used when
	// VESSEL::GetMeshTemplate() returns null on macOS.
	static std::map<std::string, MESHHANDLE> s_fallbackMeshes;
};

} // namespace ogl

#endif // !_WIN32
#endif // __OGLVVESSEL_H
