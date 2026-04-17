// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef _WIN32

#include "OGLSurface.h"
#include <cstdio>
#include <cstring>

namespace ogl {

OGLSurface::OGLSurface()
	: m_texId(0), m_fbo(0), m_depthRbo(0),
	  m_width(0), m_height(0), m_attrib(0),
	  m_refCount(1), m_ownsTexture(false),
	  m_colorKey(0), m_hasColorKey(false),
	  m_prevFBO(0)
{
}

OGLSurface::~OGLSurface()
{
	if (m_fbo) {
		glDeleteFramebuffers(1, &m_fbo);
		m_fbo = 0;
	}
	if (m_depthRbo) {
		glDeleteRenderbuffers(1, &m_depthRbo);
		m_depthRbo = 0;
	}
	if (m_ownsTexture && m_texId) {
		glDeleteTextures(1, &m_texId);
		m_texId = 0;
	}
}

bool OGLSurface::Create(DWORD w, DWORD h, DWORD attrib)
{
	m_width = w;
	m_height = h;
	m_attrib = attrib;
	m_ownsTexture = true;

	// Create the GL texture
	glGenTextures(1, &m_texId);
	glBindTexture(GL_TEXTURE_2D, m_texId);

	// Choose internal format based on attributes
	GLenum internalFmt = GL_RGBA8;
	if (attrib & OAPISURFACE_NOALPHA)
		internalFmt = GL_RGB8;

	glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	if (attrib & OAPISURFACE_MIPMAPS) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glGenerateMipmap(GL_TEXTURE_2D);
	}

	glBindTexture(GL_TEXTURE_2D, 0);

	// Pre-create FBO if this is a render target
	if (attrib & (OAPISURFACE_RENDERTARGET | OAPISURFACE_RENDER3D | OAPISURFACE_SKETCHPAD))
		EnsureFBO();

	return m_texId != 0;
}

void OGLSurface::WrapTexture(GLuint texId, DWORD w, DWORD h)
{
	// Release any previous texture we owned
	if (m_ownsTexture && m_texId && m_texId != texId)
		glDeleteTextures(1, &m_texId);

	m_texId = texId;
	m_width = w;
	m_height = h;
	m_ownsTexture = true; // OGLSurface takes ownership
	m_attrib = OAPISURFACE_TEXTURE; // default: readable texture
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

	glGenFramebuffers(1, &m_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

	// Attach the texture as color attachment
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texId, 0);

	// Create a depth renderbuffer if this is a 3D render target
	if (m_attrib & OAPISURFACE_RENDER3D) {
		glGenRenderbuffers(1, &m_depthRbo);
		glBindRenderbuffer(GL_RENDERBUFFER, m_depthRbo);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, m_width, m_height);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depthRbo);
		glBindRenderbuffer(GL_RENDERBUFFER, 0);
	}

	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE) {
		fprintf(stderr, "[OGLSurface] FBO incomplete: 0x%x (tex=%u, %ux%u)\n",
			status, m_texId, m_width, m_height);
		glDeleteFramebuffers(1, &m_fbo);
		m_fbo = 0;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return m_fbo;
}

void OGLSurface::BindFBO()
{
	if (!m_fbo) EnsureFBO();
	if (!m_fbo) return;

	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &m_prevFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
	glViewport(0, 0, m_width, m_height);
}

void OGLSurface::UnbindFBO()
{
	glBindFramebuffer(GL_FRAMEBUFFER, m_prevFBO);
	m_prevFBO = 0;
}

void OGLSurface::Fill(DWORD col)
{
	BindFBO();
	// col is 0xAARRGGBB or 0x00RRGGBB
	float r = ((col >> 16) & 0xFF) / 255.0f;
	float g = ((col >> 8)  & 0xFF) / 255.0f;
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
	// OpenGL scissor is bottom-left origin; flip y
	glScissor(x, m_height - y - h, w, h);
	float r = ((col >> 16) & 0xFF) / 255.0f;
	float g = ((col >> 8)  & 0xFF) / 255.0f;
	float b = ( col        & 0xFF) / 255.0f;
	float a = ((col >> 24) & 0xFF) / 255.0f;
	glClearColor(r, g, b, a);
	glClear(GL_COLOR_BUFFER_BIT);
	glDisable(GL_SCISSOR_TEST);
	UnbindFBO();
}

} // namespace ogl

#endif // !_WIN32
