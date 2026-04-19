// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef _WIN32

#include "OGLPostProcess.h"
#include "OGLShaderMgr.h"
#include <cstdio>

namespace ogl {

OGLPostProcess::OGLPostProcess(ShaderMgr *shaderMgr)
	: m_shaderMgr(shaderMgr), m_width(0), m_height(0), m_initialized(false),
	  m_bloomEnabled(true), m_flareEnabled(true),
	  // uExposure controls both paths. Default 1.0 passes the scene through
	  // without added gain; textured planets (Saturn, Venus, Mercury, Dione)
	  // render at their natural albedo. The previous 8× LDR workaround was
	  // needed because uExposure didn't actually reach the shader on macOS
	  // (ShaderMgr cache collision, fixed by #49) — it has been removed now
	  // that the uniform plumbing lands correctly. Caller can override via
	  // SetExposure() for HDR scenes or user preference.
	  m_bloomThreshold(0.8f), m_bloomIntensity(0.5f), m_exposure(1.0f),
	  m_sceneFBO(0), m_sceneColorTex(0), m_sceneDepthTex(0),
	  m_thresholdShader(0), m_blurShader(0), m_compositeShader(0), m_flareShader(0),
	  m_quadVAO(0), m_quadVBO(0)
{
	m_bloomFBO[0] = m_bloomFBO[1] = 0;
	m_bloomTex[0] = m_bloomTex[1] = 0;
}

OGLPostProcess::~OGLPostProcess()
{
	Release();
}

bool OGLPostProcess::Init(int width, int height)
{
	m_width = width;
	m_height = height;

	// Load shaders
	m_thresholdShader = m_shaderMgr->LoadProgram("bloom_threshold", "bloom_threshold.vert", "bloom_threshold.frag");
	m_blurShader = m_shaderMgr->LoadProgram("bloom_blur", "bloom_blur.vert", "bloom_blur.frag");
	m_compositeShader = m_shaderMgr->LoadProgram("tonemap", "tonemap.vert", "tonemap.frag");
	m_flareShader = m_shaderMgr->LoadProgram("lensflare", "lensflare.vert", "lensflare.frag");

	if (!m_thresholdShader || !m_blurShader || !m_compositeShader) {
		fprintf(stderr, "[PostProcess] Failed to load shaders — disabling\n");
		return false;
	}

	// Create fullscreen quad
	InitQuad();

	// Create HDR scene FBO (full resolution, float16)
	glGenFramebuffers(1, &m_sceneFBO);
	glGenTextures(1, &m_sceneColorTex);
	glGenTextures(1, &m_sceneDepthTex);

	glBindTexture(GL_TEXTURE_2D, m_sceneColorTex);
	// macOS OpenGL 4.1-via-Metal silently drops fragment writes to
	// floating-point colour attachments (GL_RGBA16F, GL_R11F_G11F_B10F):
	// glCheckFramebufferStatus reports COMPLETE and no GL error fires,
	// but the attachment stays all-zeros. That produced the "black scene
	// with HUD only" failure mode that invalidated the Phase 1 smoke
	// battery and the M30 rendering_parity baselines.
	//
	// Default to GL_RGBA8 on macOS: we lose HDR headroom (bloom threshold
	// clamps to [0,1] so only the sun disc gets a real highlight), but
	// the scene renders at all. OGL_POSTFX_HDR=1 re-enables GL_RGBA16F
	// for validation on hardware / driver combinations that do support
	// float colour attachments (e.g. a future macOS GL stack, or Linux
	// Mesa drivers once the port adds Linux support).
	static const bool s_forceHdr = []() {
		const char *v = std::getenv("OGL_POSTFX_HDR");
		return v && v[0] == '1';
	}();
	if (s_forceHdr) {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
		             width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
	} else {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
		             width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	}
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// Attach depth as a texture (GL_DEPTH_COMPONENT24) rather than a
	// renderbuffer so the tonemap composite shader can sample it to mask
	// bloom contribution at foreground pixels (issue #71). Atmospheric
	// pixels are drawn with depth writes off so they keep the cleared
	// 1.0 depth; vessels write their actual depth. A depth check in the
	// composite can therefore distinguish "far / atm / sun" from
	// "foreground / vessel" and only apply bloom to the former.
	glBindTexture(GL_TEXTURE_2D, m_sceneDepthTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24,
	             width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glBindFramebuffer(GL_FRAMEBUFFER, m_sceneFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_sceneColorTex, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_sceneDepthTex, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		fprintf(stderr, "[PostProcess] Scene FBO incomplete\n");
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		return false;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Create bloom FBOs (half resolution)
	int bw = width / 2, bh = height / 2;
	for (int i = 0; i < 2; i++) {
		if (!CreateFBO(m_bloomFBO[i], m_bloomTex[i], bw, bh, GL_RGBA16F)) {
			fprintf(stderr, "[PostProcess] Bloom FBO %d incomplete\n", i);
			return false;
		}
	}

	m_initialized = true;
	fprintf(stderr, "[PostProcess] Initialized %dx%d (bloom %dx%d)\n", width, height, bw, bh);
	return true;
}

void OGLPostProcess::Resize(int width, int height)
{
	if (width == m_width && height == m_height) return;
	Release();
	Init(width, height);
}

void OGLPostProcess::Release()
{
	if (m_sceneFBO) { glDeleteFramebuffers(1, &m_sceneFBO); m_sceneFBO = 0; }
	if (m_sceneColorTex) { glDeleteTextures(1, &m_sceneColorTex); m_sceneColorTex = 0; }
	if (m_sceneDepthTex) { glDeleteTextures(1, &m_sceneDepthTex); m_sceneDepthTex = 0; }
	for (int i = 0; i < 2; i++) {
		if (m_bloomFBO[i]) { glDeleteFramebuffers(1, &m_bloomFBO[i]); m_bloomFBO[i] = 0; }
		if (m_bloomTex[i]) { glDeleteTextures(1, &m_bloomTex[i]); m_bloomTex[i] = 0; }
	}
	if (m_quadVAO) { glDeleteVertexArrays(1, &m_quadVAO); m_quadVAO = 0; }
	if (m_quadVBO) { glDeleteBuffers(1, &m_quadVBO); m_quadVBO = 0; }
	m_initialized = false;
}

bool OGLPostProcess::CreateFBO(GLuint &fbo, GLuint &tex, int w, int h, GLenum internalFormat)
{
	glGenFramebuffers(1, &fbo);
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	// Apply the same macOS HDR-format workaround documented in Init():
	// bloom / lens-flare FBOs also need to avoid GL_RGBA16F on Apple Silicon
	// OpenGL. The caller's RGBA8 path is preserved verbatim.
	static const bool s_forceHdr = []() {
		const char *v = std::getenv("OGL_POSTFX_HDR");
		return v && v[0] == '1';
	}();
	GLenum format = GL_RGBA, type = GL_UNSIGNED_BYTE;
	if (internalFormat == GL_RGBA16F) {
		if (!s_forceHdr) internalFormat = GL_RGBA8;
		else { format = GL_RGBA; type = GL_FLOAT; }
	}
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, w, h, 0, format, type, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
	bool ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return ok;
}

void OGLPostProcess::InitQuad()
{
	float verts[] = {
		// pos      uv
		-1, -1,   0, 0,
		 1, -1,   1, 0,
		-1,  1,   0, 1,
		 1,  1,   1, 1,
	};
	glGenVertexArrays(1, &m_quadVAO);
	glGenBuffers(1, &m_quadVBO);
	glBindVertexArray(m_quadVAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
	glEnableVertexAttribArray(1);
	glBindVertexArray(0);
}

void OGLPostProcess::DrawQuad()
{
	glBindVertexArray(m_quadVAO);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray(0);
}

void OGLPostProcess::BeginScene()
{
	if (!m_initialized) return;
	glBindFramebuffer(GL_FRAMEBUFFER, m_sceneFBO);
	glViewport(0, 0, m_width, m_height);
	glClearColor(0.0f, 0.0f, 0.01f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void OGLPostProcess::EndScene(float sunScreenX, float sunScreenY, bool sunVisible)
{
	if (!m_initialized) {
		return;
	}

	// Apply bloom if enabled (OGL_POSTFX_NOBLOOM=1 skips it for diagnostics)
	static const bool s_noBloom = []() {
		const char *v = std::getenv("OGL_POSTFX_NOBLOOM");
		return v && v[0] == '1';
	}();
	if (m_bloomEnabled && !s_noBloom)
		ApplyBloom();

	// Apply lens flare if enabled
	if (m_flareEnabled && m_flareShader)
		ApplyLensFlare(sunScreenX, sunScreenY, sunVisible);

	// Final composite: tone map HDR scene + bloom to screen
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, m_width, m_height);
	// Reset all blend/depth/stencil/colour-mask state so upstream passes
	// (vessel PBR, shadow map, particle system, ImGui during Launchpad)
	// can't leave the compositor with a blend factor that blacks-out the
	// fullscreen quad.
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_CULL_FACE);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDepthMask(GL_FALSE);

	glUseProgram(m_compositeShader);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_sceneColorTex);
	glUniform1i(m_shaderMgr->GetUniformLoc(m_compositeShader, "uScene"), 0);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, m_bloomEnabled ? m_bloomTex[0] : 0);
	glUniform1i(m_shaderMgr->GetUniformLoc(m_compositeShader, "uBloom"), 1);

	// Scene depth (issue #71): foreground pixels (written by the vessel
	// pass) have depth < 1.0; background pixels (atmosphere fullscreen
	// quad + distance-normalised planet + stars) kept the cleared 1.0.
	// The shader uses that to mask bloom spill onto vessel silhouettes
	// at the limb, where the bright atm halo would otherwise bleed
	// through the hull via the bloom blur kernel.
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, m_sceneDepthTex);
	glUniform1i(m_shaderMgr->GetUniformLoc(m_compositeShader, "uSceneDepth"), 2);

	glUniform1f(m_shaderMgr->GetUniformLoc(m_compositeShader, "uExposure"), m_exposure);
	glUniform1f(m_shaderMgr->GetUniformLoc(m_compositeShader, "uBloomIntensity"),
		m_bloomEnabled && !s_noBloom ? m_bloomIntensity : 0.0f);
	// Tell the composite shader whether the scene attachment is HDR
	// (R11F/R16F etc.) or LDR (RGBA8 fallback). ACES clamps dark pixels
	// to 0 when fed LDR values, so the shader needs to skip the curve
	// in that case and just gamma-encode.
	{
		const char *hdrEnv = std::getenv("OGL_POSTFX_HDR");
		bool isHdr = hdrEnv && hdrEnv[0] == '1';
		glUniform1i(m_shaderMgr->GetUniformLoc(m_compositeShader, "uIsHDR"),
			isHdr ? 1 : 0);
	}

	DrawQuad();

	glUseProgram(0);
	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);
}

void OGLPostProcess::ApplyBloom()
{
	int bw = m_width / 2, bh = m_height / 2;

	glDisable(GL_DEPTH_TEST);

	// Step 1: Extract bright pixels from scene → bloom FBO 0
	glBindFramebuffer(GL_FRAMEBUFFER, m_bloomFBO[0]);
	glViewport(0, 0, bw, bh);
	glUseProgram(m_thresholdShader);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_sceneColorTex);
	glUniform1i(m_shaderMgr->GetUniformLoc(m_thresholdShader, "uScene"), 0);
	glUniform1f(m_shaderMgr->GetUniformLoc(m_thresholdShader, "uThreshold"), m_bloomThreshold);
	DrawQuad();

	// Step 2: Gaussian blur (ping-pong between FBOs 0 and 1)
	int blurPasses = 4; // 2 horizontal + 2 vertical = 4 passes
	glUseProgram(m_blurShader);
	for (int i = 0; i < blurPasses; i++) {
		bool horizontal = (i % 2 == 0);
		int srcFBO = i % 2;
		int dstFBO = 1 - srcFBO;

		glBindFramebuffer(GL_FRAMEBUFFER, m_bloomFBO[dstFBO]);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m_bloomTex[srcFBO]);
		glUniform1i(m_shaderMgr->GetUniformLoc(m_blurShader, "uImage"), 0);
		glUniform1i(m_shaderMgr->GetUniformLoc(m_blurShader, "uHorizontal"), horizontal ? 1 : 0);
		DrawQuad();
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glEnable(GL_DEPTH_TEST);
}

void OGLPostProcess::ApplyLensFlare(float sunX, float sunY, bool sunVisible)
{
	if (!sunVisible || !m_flareShader) return;

	glBindFramebuffer(GL_FRAMEBUFFER, m_sceneFBO);
	glViewport(0, 0, m_width, m_height);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE); // additive

	glUseProgram(m_flareShader);
	// Sun position in UV space [0,1]
	float sunU = sunX * 0.5f + 0.5f;
	float sunV = sunY * 0.5f + 0.5f;
	glUniform2f(m_shaderMgr->GetUniformLoc(m_flareShader, "uSunPos"), sunU, sunV);
	glUniform1f(m_shaderMgr->GetUniformLoc(m_flareShader, "uIntensity"), 0.3f);
	glUniform2f(m_shaderMgr->GetUniformLoc(m_flareShader, "uResolution"),
		(float)m_width, (float)m_height);
	DrawQuad();

	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
}

} // namespace ogl

#endif // !_WIN32
