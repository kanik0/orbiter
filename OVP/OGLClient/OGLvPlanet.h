// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// OGLvPlanet - Planet/moon rendering (textured spheres, rings)

#ifndef __OGLVPLANET_H
#define __OGLVPLANET_H

#ifndef _WIN32
#include "OGLvObject.h"
#include <OpenGL/gl3.h>
#include <string>

struct OGLTexture;

namespace ogl {

class OGLTileMgr;
class OGLAtmosphere;

class OGLvPlanet : public OGLvObject {
public:
	OGLvPlanet(OBJHANDLE hObj, ShaderMgr *shaderMgr);
	~OGLvPlanet() override;

	// One-time init: create sphere geometry and load shaders.
	// texturePath = base directory for texture files.
	static void InitShared(ShaderMgr *shaderMgr, const std::string &texturePath);
	static void ReleaseShared();

	// Load the texture for this planet (call after InitShared)
	void LoadTexture(const std::string &texturePath);

	// Look for <PlanetName>_cloud.{tex,dds,bmp}; if one is found a cloud
	// sphere is rendered one shell above the surface every frame.
	void LoadCloudTexture(const std::string &texturePath);

	// Initialize LOD tile manager for this planet (call after InitShared)
	void InitTiles(const std::string &texturePath);

	void Render(const float *vp, const VECTOR3 &camPos, const VECTOR3 &sunPos) override;

private:
	OGLTexture *m_texture;   // planet surface texture (null → flat-color fallback)
	OGLTexture *m_cloudTex;  // cloud layer texture   (null → no clouds)
	OGLTileMgr *m_tileMgr;   // LOD tile manager      (null → no .tree archives)
	OGLAtmosphere *m_atmo;   // atmospheric haze      (null → no atmosphere)

	// Render the planetary rings if this body has them
	void RenderRings(const float *vp, const VECTOR3 &camPos, const VECTOR3 &sunPos);

	// Render the cloud shell (identical sphere geometry to the textured
	// planet mesh, just scaled up a touch). Depth-test reads but does not
	// write so the atmosphere pass downstream still composes correctly.
	void RenderClouds(const float *vp, const VECTOR3 &camPos, const VECTOR3 &sunPos);

	// --- Shared resources (static, created once for all planets) ---

	// Flat-color sphere
	static GLuint s_planetShader;
	static GLuint s_sphereVAO, s_sphereVBO, s_sphereEBO;
	static int s_sphereIndexCount;

	// Textured sphere
	static GLuint s_texPlanetShader;
	static GLuint s_texSphereVAO, s_texSphereVBO, s_texSphereEBO;
	static int s_texSphereIndexCount;

	// Cloud shell (re-uses the textured sphere geometry; different shader).
	static GLuint s_cloudShader;

	// Ring rendering
	static GLuint s_ringShader;
	static GLuint s_ringVAO, s_ringVBO, s_ringEBO;
	static int s_ringIndexCount;
	static OGLTexture *s_ringTexture;
	static bool s_ringsInitialized;

	static void InitRingsShared(ShaderMgr *shaderMgr, const std::string &texturePath);

	static bool s_sharedInitialized;
	static ShaderMgr *s_shaderMgr;
};

} // namespace ogl

#endif // !_WIN32
#endif // __OGLVPLANET_H
