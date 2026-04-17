// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// OGLCelSphere - Celestial sphere (starfield) rendering

#ifndef __OGLCELSPHERE_H
#define __OGLCELSPHERE_H

#ifndef _WIN32
#include <OpenGL/gl3.h>

namespace ogl {

class ShaderMgr;

class OGLCelSphere {
public:
	OGLCelSphere(ShaderMgr *shaderMgr);
	~OGLCelSphere();

	// Initialize star geometry and shaders. Call once after GL context is ready.
	void Init(int numStars);

	// Render the starfield + solar corona. `vp` is the rotation-only VP
	// used for the stars. `time` drives the subtle twinkle animation;
	// `sunNdcX/Y` carry the sun's projected position (used by the corona
	// pass — set `sunVisible=false` when the sun is behind the camera).
	void Render(const float *vp, float time,
	            float sunNdcX, float sunNdcY, bool sunVisible,
	            int viewW, int viewH);

	// Release GL resources
	void Release();

private:
	ShaderMgr *m_shaderMgr;
	GLuint m_vao, m_vbo;
	GLuint m_shader;
	int    m_numStars;

	GLuint m_coronaVAO;       // empty VAO (vertices from gl_VertexID)
	GLuint m_coronaShader;

	// Planetarium grid (equatorial + ecliptic). Built once at Init, drawn
	// only when OGL_PLANETARIUM=1 is set in the environment (matches the
	// typical F9 toggle in the Windows HUD without requiring the keymap
	// wiring to land first).
	GLuint m_gridVAO, m_gridVBO;
	GLuint m_gridShader;
	int    m_gridVertCount;
	bool   m_gridEnabled;

	void BuildGrid();
};

} // namespace ogl

#endif // !_WIN32
#endif // __OGLCELSPHERE_H
