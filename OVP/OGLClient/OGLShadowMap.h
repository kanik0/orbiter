// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// OGLShadowMap - Directional shadow map for vessel self-shadowing.
//
// One shared 1024x1024 depth texture feeds the vessel pass; before rendering
// its colour output a vessel calls BeginPass() to bind the depth FBO, draws
// every mesh group with s_shadowShader, then EndPass() restores the prior
// framebuffer. The main pass samples the same texture at unit 3 and the
// matching light-space VP matrix via shadow.glsl's shadowPCF3x3().
//
// The light volume is a tight orthographic box around the focus vessel — the
// plan calls for two cascades (50 m + 2 km) and we'll grow into that in a
// follow-up; the current single cascade is already enough to cast the hard
// shadow signature Atlantis needs on approach.

#ifndef __OGLSHADOWMAP_H
#define __OGLSHADOWMAP_H

#ifndef _WIN32
#include "OrbiterAPI.h"
#include <OpenGL/gl3.h>

namespace ogl {

class OGLShadowMap {
public:
	OGLShadowMap();
	~OGLShadowMap();

	// Allocate the depth texture + FBO. Returns false on GL failure.
	bool Init(int size = 1024);
	void Release();

	// Recompute the light-space view-projection using `sunDir` (world-space,
	// unit) and an orthographic volume `halfExtent` metres on a side centred
	// at `targetPos` (world-space). `sunDist` sets the distance of the light
	// frustum's near plane from targetPos along -sunDir; `farDist` sets the
	// far plane.
	void BuildLightMatrix(const VECTOR3 &sunDir, const VECTOR3 &targetPos,
	                      double halfExtent, double sunDist, double farDist);

	// Bind for the shadow pass. Saves the current draw framebuffer and the
	// viewport; restored by EndPass().
	void BeginPass();
	void EndPass();

	// Accessors used by the colour pass.
	GLuint      GetDepthTexture() const { return m_depthTex; }
	const float *GetLightVP()     const { return m_lightVP; }
	int         GetSize()         const { return m_size; }

private:
	GLuint m_fbo;
	GLuint m_depthTex;
	int    m_size;

	float m_lightVP[16];      // view * proj in column-major

	GLint m_prevFBO;
	GLint m_prevViewport[4];
};

} // namespace ogl

#endif // !_WIN32
#endif // __OGLSHADOWMAP_H
