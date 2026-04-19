// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// OGLPostProcess - Fullscreen post-processing pipeline
// HDR rendering, bloom extraction, gaussian blur, tone mapping, lens flare

#ifndef __OGLPOSTPROCESS_H
#define __OGLPOSTPROCESS_H

#ifndef _WIN32
#include <OpenGL/gl3.h>

namespace ogl {

class ShaderMgr;

class OGLPostProcess {
public:
	OGLPostProcess(ShaderMgr *shaderMgr);
	~OGLPostProcess();

	// Initialize FBOs and shaders for given viewport size
	bool Init(int width, int height);

	// Resize FBOs when viewport changes
	void Resize(int width, int height);

	// Release all resources
	void Release();

	// --- Rendering pipeline ---

	// Step 1: Bind the HDR scene FBO. All 3D rendering goes here.
	void BeginScene();

	// Step 2: End scene rendering, apply post-processing, output to screen.
	// sunScreenX/Y: sun position in NDC [-1,1] for lens flare (or NaN if not visible)
	void EndScene(float sunScreenX, float sunScreenY, bool sunVisible);

	// --- Configuration ---

	void SetBloomEnabled(bool e) { m_bloomEnabled = e; }
	void SetBloomThreshold(float t) { m_bloomThreshold = t; }
	void SetBloomIntensity(float i) { m_bloomIntensity = i; }
	void SetLensFlareEnabled(bool e) { m_flareEnabled = e; }
	void SetExposure(float e) { m_exposure = e; }

	bool IsEnabled() const { return m_initialized && (m_bloomEnabled || m_flareEnabled); }

private:
	ShaderMgr *m_shaderMgr;
	int m_width, m_height;
	bool m_initialized;

	// Configuration
	bool m_bloomEnabled;
	bool m_flareEnabled;
	float m_bloomThreshold;
	float m_bloomIntensity;
	float m_exposure;

	// HDR scene FBO (full resolution, RGBA16F)
	GLuint m_sceneFBO;
	GLuint m_sceneColorTex;
	// Depth attached as a texture (not a renderbuffer) so the tonemap
	// composite can sample it to mask bloom contribution at foreground
	// pixels — see issue #71.
	GLuint m_sceneDepthTex;

	// Bloom FBOs (half resolution, ping-pong for blur)
	GLuint m_bloomFBO[2];
	GLuint m_bloomTex[2];

	// Shaders
	GLuint m_thresholdShader;   // extract bright pixels
	GLuint m_blurShader;        // gaussian blur (horizontal/vertical)
	GLuint m_compositeShader;   // combine scene + bloom + tone map
	GLuint m_flareShader;       // lens flare overlay

	// Fullscreen quad
	GLuint m_quadVAO, m_quadVBO;

	void InitQuad();
	void DrawQuad();

	// Create an FBO with a color texture attachment
	bool CreateFBO(GLuint &fbo, GLuint &tex, int w, int h, GLenum internalFormat);

	// Bloom pass: threshold → blur → composite
	void ApplyBloom();

	// Lens flare pass
	void ApplyLensFlare(float sunX, float sunY, bool sunVisible);
};

} // namespace ogl

#endif // !_WIN32
#endif // __OGLPOSTPROCESS_H
