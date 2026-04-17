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

	// Render the starfield. vp = view-projection (rotation only, no translation).
	void Render(const float *vp);

	// Release GL resources
	void Release();

private:
	ShaderMgr *m_shaderMgr;
	GLuint m_vao, m_vbo;
	GLuint m_shader;
	int m_numStars;
};

} // namespace ogl

#endif // !_WIN32
#endif // __OGLCELSPHERE_H
