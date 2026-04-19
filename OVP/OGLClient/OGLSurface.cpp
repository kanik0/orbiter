// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef _WIN32

#include "OGLSurface.h"
#include <cstdio>
#include <cstring>

namespace ogl {

// ---------------------------------------------------------------------------
// FBOBinder
// ---------------------------------------------------------------------------

FBOBinder::FBOBinder(GLuint fbo, int w, int h)
	: m_prevDraw(0), m_prevRead(0), m_restoreVp(false)
{
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &m_prevDraw);
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &m_prevRead);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	if (w > 0 && h > 0) {
		glGetIntegerv(GL_VIEWPORT, m_prevVp);
		glViewport(0, 0, w, h);
		m_restoreVp = true;
	}
}

FBOBinder::~FBOBinder()
{
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)m_prevDraw);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)m_prevRead);
	if (m_restoreVp)
		glViewport(m_prevVp[0], m_prevVp[1], m_prevVp[2], m_prevVp[3]);
}

// ---------------------------------------------------------------------------
// OGLSurface
// ---------------------------------------------------------------------------

OGLSurface::OGLSurface()
	: m_texId(0), m_fbo(0), m_msaaFbo(0),
	  m_msaaColorRbo(0), m_msaaDepthRbo(0), m_depthRbo(0),
	  m_width(0), m_height(0), m_attrib(0),
	  m_refCount(1), m_samples(0), m_hasStencil(false),
	  m_ownsTexture(false), m_fboFailed(false),
	  m_colorKey(0), m_hasColorKey(false),
	  m_prevDrawFBO(0), m_prevReadFBO(0)
{
	m_prevViewport[0] = m_prevViewport[1] = 0;
	m_prevViewport[2] = m_prevViewport[3] = 0;
}

OGLSurface::~OGLSurface()
{
	if (m_msaaFbo)      { glDeleteFramebuffers(1, &m_msaaFbo);      m_msaaFbo = 0; }
	if (m_msaaColorRbo) { glDeleteRenderbuffers(1, &m_msaaColorRbo); m_msaaColorRbo = 0; }
	if (m_msaaDepthRbo) { glDeleteRenderbuffers(1, &m_msaaDepthRbo); m_msaaDepthRbo = 0; }
	if (m_fbo)          { glDeleteFramebuffers(1, &m_fbo);          m_fbo = 0; }
	if (m_depthRbo)     { glDeleteRenderbuffers(1, &m_depthRbo);    m_depthRbo = 0; }
	if (m_ownsTexture && m_texId) {
		glDeleteTextures(1, &m_texId);
		m_texId = 0;
	}
}

bool OGLSurface::Create(DWORD w, DWORD h, DWORD attrib)
{
	// RENDER3D wants depth+stencil; everything else defaults to no stencil.
	const bool wantStencil = (attrib & OAPISURFACE_RENDER3D) != 0;
	return CreateEx(w, h, attrib, 0, wantStencil);
}

bool OGLSurface::CreateEx(DWORD w, DWORD h, DWORD attrib, int samples, bool wantStencil)
{
	m_width        = w;
	m_height       = h;
	m_attrib       = attrib;
	m_samples      = samples;
	m_hasStencil   = wantStencil;
	m_ownsTexture  = true;

	// Create the resolve / sampled texture. Format is always RGBA8 — Orbiter
	// legacy code sometimes requests 16-bit RGB565 targets (most visibly the
	// Virtual Cockpit MFD panels), which used to crash the client; RGBA8 is
	// strictly a superset and matches every sampler declaration in our shaders.
	GLenum internalFmt = (attrib & OAPISURFACE_NOALPHA) ? GL_RGB8 : GL_RGBA8;

	glGenTextures(1, &m_texId);
	glBindTexture(GL_TEXTURE_2D, m_texId);
	glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

	if (attrib & OAPISURFACE_MIPMAPS) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glGenerateMipmap(GL_TEXTURE_2D);
	} else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

	const DWORD rtMask = OAPISURFACE_RENDERTARGET | OAPISURFACE_RENDER3D | OAPISURFACE_SKETCHPAD;
	if (attrib & rtMask) {
		EnsureFBO();
		fprintf(stderr,
		        "[OGLSurface] RT %ux%u attrib=0x%04lx samples=%d stencil=%d mips=%d\n",
		        (unsigned)w, (unsigned)h, (unsigned long)attrib,
		        m_samples, (int)m_hasStencil, (int)HasMipmaps());
	}

	return m_texId != 0;
}

void OGLSurface::WrapTexture(GLuint texId, DWORD w, DWORD h)
{
	if (m_ownsTexture && m_texId && m_texId != texId)
		glDeleteTextures(1, &m_texId);

	m_texId       = texId;
	m_width       = w;
	m_height      = h;
	m_ownsTexture = true;
	m_attrib      = OAPISURFACE_TEXTURE;
}

int OGLSurface::Release()
{
	int rc = --m_refCount;
	if (rc <= 0) {
		delete this;
		return 0;
	}
	return rc;
}

GLuint OGLSurface::EnsureFBO()
{
	if (m_fbo) return m_fbo;
	if (!m_texId) return 0;
	if (m_fboFailed) return 0;

	// --- Single-sample resolve FBO (always built). ---------------------------
	glGenFramebuffers(1, &m_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texId, 0);

	// Depth/stencil for the resolve target is only useful when we're the sole
	// render path (m_samples==0). The MSAA path keeps its own depth RBO below.
	if (m_samples == 0 && (m_attrib & (OAPISURFACE_RENDER3D | OAPISURFACE_RENDERTARGET))) {
		glGenRenderbuffers(1, &m_depthRbo);
		glBindRenderbuffer(GL_RENDERBUFFER, m_depthRbo);
		GLenum depthFmt = m_hasStencil ? GL_DEPTH24_STENCIL8 : GL_DEPTH_COMPONENT24;
		glRenderbufferStorage(GL_RENDERBUFFER, depthFmt, m_width, m_height);
		GLenum attachment = m_hasStencil ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT;
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, attachment, GL_RENDERBUFFER, m_depthRbo);
		glBindRenderbuffer(GL_RENDERBUFFER, 0);
	}

	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE) {
		fprintf(stderr, "[OGLSurface] Resolve FBO incomplete: 0x%x (tex=%u, %ux%u, attrib=0x%lx, stencil=%d)\n",
		        status, m_texId, m_width, m_height, (unsigned long)m_attrib, (int)m_hasStencil);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDeleteFramebuffers(1, &m_fbo);
		m_fbo = 0;
		m_fboFailed = true;  // don't retry every BindFBO call
		return 0;
	}

	// --- Multisample FBO (optional). -----------------------------------------
	if (m_samples > 0) {
		glGenFramebuffers(1, &m_msaaFbo);
		glBindFramebuffer(GL_FRAMEBUFFER, m_msaaFbo);

		glGenRenderbuffers(1, &m_msaaColorRbo);
		glBindRenderbuffer(GL_RENDERBUFFER, m_msaaColorRbo);
		GLenum colorFmt = (m_attrib & OAPISURFACE_NOALPHA) ? GL_RGB8 : GL_RGBA8;
		glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_samples, colorFmt, m_width, m_height);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_msaaColorRbo);

		if (m_attrib & (OAPISURFACE_RENDER3D | OAPISURFACE_RENDERTARGET)) {
			glGenRenderbuffers(1, &m_msaaDepthRbo);
			glBindRenderbuffer(GL_RENDERBUFFER, m_msaaDepthRbo);
			GLenum depthFmt = m_hasStencil ? GL_DEPTH24_STENCIL8 : GL_DEPTH_COMPONENT24;
			glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_samples, depthFmt, m_width, m_height);
			GLenum attachment = m_hasStencil ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT;
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, attachment, GL_RENDERBUFFER, m_msaaDepthRbo);
		}
		glBindRenderbuffer(GL_RENDERBUFFER, 0);

		status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			fprintf(stderr, "[OGLSurface] MSAA FBO incomplete: 0x%x (samples=%d, %ux%u)\n",
			        status, m_samples, m_width, m_height);
			glDeleteFramebuffers(1, &m_msaaFbo);
			glDeleteRenderbuffers(1, &m_msaaColorRbo);
			if (m_msaaDepthRbo) glDeleteRenderbuffers(1, &m_msaaDepthRbo);
			m_msaaFbo = m_msaaColorRbo = m_msaaDepthRbo = 0;
			m_samples = 0;  // degrade gracefully to no-MSAA path
		}
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return m_fbo;
}

void OGLSurface::BindFBO()
{
	if (!m_fbo) EnsureFBO();
	if (!m_fbo) return;

	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &m_prevDrawFBO);
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &m_prevReadFBO);
	glGetIntegerv(GL_VIEWPORT, m_prevViewport);

	// Draw into the MSAA FBO when available; resolve happens in UnbindFBO.
	GLuint target = (m_samples > 0 && m_msaaFbo) ? m_msaaFbo : m_fbo;
	glBindFramebuffer(GL_FRAMEBUFFER, target);
	glViewport(0, 0, (GLsizei)m_width, (GLsizei)m_height);
}

void OGLSurface::UnbindFBO()
{
	// 1. Resolve multisample → resolve texture.
	if (m_samples > 0 && m_msaaFbo && m_fbo) {
		glBindFramebuffer(GL_READ_FRAMEBUFFER, m_msaaFbo);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo);
		glBlitFramebuffer(0, 0, (GLint)m_width, (GLint)m_height,
		                  0, 0, (GLint)m_width, (GLint)m_height,
		                  GL_COLOR_BUFFER_BIT, GL_NEAREST);
	}

	// 2. Regenerate mipmaps so sampled reads see the freshly-rendered content.
	if ((m_attrib & OAPISURFACE_MIPMAPS) && m_texId) {
		glBindTexture(GL_TEXTURE_2D, m_texId);
		glGenerateMipmap(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	// 3. Restore prior bindings + viewport.
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)m_prevDrawFBO);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)m_prevReadFBO);
	glViewport(m_prevViewport[0], m_prevViewport[1],
	           m_prevViewport[2], m_prevViewport[3]);

	m_prevDrawFBO = m_prevReadFBO = 0;
}

void OGLSurface::Fill(DWORD col)
{
	BindFBO();
	float r = ((col >> 16) & 0xFF) / 255.0f;
	float g = ((col >>  8) & 0xFF) / 255.0f;
	float b = ( col        & 0xFF) / 255.0f;
	float a = ((col >> 24) & 0xFF) / 255.0f;
	glClearColor(r, g, b, a);
	glClear(GL_COLOR_BUFFER_BIT);
	UnbindFBO();
}

void OGLSurface::Fill(DWORD x, DWORD y, DWORD w, DWORD h, DWORD col)
{
	BindFBO();
	glEnable(GL_SCISSOR_TEST);
	// OpenGL scissor uses bottom-left origin, so flip y.
	glScissor((GLint)x, (GLint)(m_height - y - h), (GLsizei)w, (GLsizei)h);
	float r = ((col >> 16) & 0xFF) / 255.0f;
	float g = ((col >>  8) & 0xFF) / 255.0f;
	float b = ( col        & 0xFF) / 255.0f;
	float a = ((col >> 24) & 0xFF) / 255.0f;
	glClearColor(r, g, b, a);
	glClear(GL_COLOR_BUFFER_BIT);
	glDisable(GL_SCISSOR_TEST);
	UnbindFBO();
}

} // namespace ogl

#endif // !_WIN32
