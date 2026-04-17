// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// OGLScene - Scene manager that orchestrates all rendering

#ifndef __OGLSCENE_H
#define __OGLSCENE_H

#ifndef _WIN32
#include "OrbiterAPI.h"
#include <OpenGL/gl3.h>
#include <string>
#include <vector>

namespace ogl {

class ShaderMgr;
class OGLCelSphere;
class OGLvPlanet;
class OGLvVessel;
class OGLEnvMap;

class OGLScene {
public:
	OGLScene(ShaderMgr *shaderMgr);
	~OGLScene();

	OGLEnvMap *GetEnvMap() const { return m_envMap; }

	// Sun position in NDC coordinates as projected by the last frame's VP.
	// `visible` is false when the sun ended up behind the camera.
	void GetSunNDC(float &x, float &y, bool &visible) const {
		x = m_lastSunNDC[0]; y = m_lastSunNDC[1]; visible = m_lastSunVisible;
	}

	// Initialize the scene: create shared resources, build object lists.
	// texturePath = base directory for textures.
	void Init(const std::string &texturePath);

	// Main render callback — called from OGLClient::clbkRenderScene
	void RenderScene(DWORD viewW, DWORD viewH);

	// Release all resources
	void Release();

private:
	ShaderMgr *m_shaderMgr;
	std::string m_texturePath;

	// Celestial sphere (starfield)
	OGLCelSphere *m_celSphere;

	// IBL environment (prefiltered cubemap shared across every vessel).
	OGLEnvMap *m_envMap;
	bool m_envMapBaked;

	// Visual objects
	std::vector<OGLvPlanet*> m_planets;
	std::vector<OGLvVessel*> m_vessels;

	// Sun screen position cached each frame for the post-process lens-flare
	// pass. (-1..1) in NDC; visible=false when behind the camera.
	float m_lastSunNDC[2];
	bool  m_lastSunVisible;

	bool m_initialized;

	// Build the view-projection matrix from Orbiter camera state
	void BuildViewProjection(DWORD viewW, DWORD viewH,
	                         float *vp, VECTOR3 &camPos, MATRIX3 &camRot);

	// Populate planet/vessel lists from the simulation on first frame
	void PopulateObjects();
	bool m_objectsPopulated;
};

} // namespace ogl

#endif // !_WIN32
#endif // __OGLSCENE_H
