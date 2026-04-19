// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// OGLSurface - Surface wrapper for OGLClient.
//
// Encapsulates a GL texture with optional FBO (render target), color key
// and reference counting; it is the concrete type behind SURFHANDLE.
//
// Beyond the plain single-sampled render-to-texture path it supports:
//   - Multi-sampled FBOs (m_samples > 0) that resolve to the public texture
//     on UnbindFBO via glBlitFramebuffer.
//   - Automatic glGenerateMipmap on UnbindFBO for OAPISURFACE_MIPMAPS targets.
//   - Stencil attachment (GL_DEPTH24_STENCIL8 vs GL_DEPTH_COMPONENT24).
//   - 16-bit format attribute hints are silently promoted to RGBA8 — the VC
//     pipeline historically asked for RGB565 surfaces which crashed the
//     pre-M1 client; RGBA8 is correct for every sampler we bind.
//
// For state-leak safety, callers that bind arbitrary FBOs should prefer the
// ogl::FBOBinder RAII helper below; BindFBO/UnbindFBO on an OGLSurface still
// works but requires matched pairs.

#ifndef __OGLSURFACE_H
#define __OGLSURFACE_H

#ifndef _WIN32
#include <OpenGL/gl3.h>
#include "OrbiterAPI.h"

namespace ogl {

class OGLSurface {
public:
	OGLSurface();
	~OGLSurface();

	// --- Creation -----------------------------------------------------------

	// Create an empty surface with the given size and OAPISURFACE_* attribute
	// mask. Render-target attributes (RENDERTARGET, RENDER3D, SKETCHPAD)
	// eagerly allocate the FBO; plain textures allocate it lazily when
	// EnsureFBO() is first called.
	bool Create(DWORD w, DWORD h, DWORD attrib);

	// As Create() but with explicit MSAA sample count (0, 2, 4, 8) and an
	// explicit request for a stencil attachment. Only meaningful together
	// with a render-target attribute.
	bool CreateEx(DWORD w, DWORD h, DWORD attrib, int samples, bool wantStencil);

	// Wrap an existing GL texture ID (takes ownership). Used when loading
	// textures from files via OGLTexture.
	void WrapTexture(GLuint texId, DWORD w, DWORD h);

	// --- Properties ---------------------------------------------------------

	GLuint GetTexture() const { return m_texId; }
	DWORD  GetWidth() const { return m_width; }
	DWORD  GetHeight() const { return m_height; }
	DWORD  GetAttrib() const { return m_attrib; }
	// Merge additional attribute flags into m_attrib. Used by clbkLoadSurface
	// to promote a file-loaded (wrapped) texture to render-target usage so
	// EnsureFBO() attaches the depth renderbuffer required by the FBO.
	void   AddAttrib(DWORD extra) { m_attrib |= extra; }
	bool   IsRenderTarget() const { return m_fbo != 0 || m_msaaFbo != 0; }
	int    GetSamples() const { return m_samples; }
	bool   HasStencil() const { return m_hasStencil; }
	bool   HasMipmaps() const { return (m_attrib & OAPISURFACE_MIPMAPS) != 0; }

	// --- Reference counting -------------------------------------------------

	void AddRef() { m_refCount++; }
	int  Release();                    // returns new ref count; deletes self on 0
	int  GetRefCount() const { return m_refCount; }

	// --- FBO for render-to-texture -----------------------------------------

	// Lazily allocate the FBO (and, when m_samples>0, the multisample FBO).
	// Returns the single-sample draw FBO id, or 0 on failure.
	GLuint EnsureFBO();

	// Bind the surface for rendering: multisample FBO when MSAA, otherwise
	// the single-sample FBO. Previous framebuffer/viewport are saved and
	// restored by the matching UnbindFBO() call.
	void BindFBO();

	// Restore the framebuffer/viewport that were active at BindFBO() time.
	// Also resolves MSAA into the resolve texture and regenerates mipmaps
	// when the surface requested OAPISURFACE_MIPMAPS.
	void UnbindFBO();

	// --- Color key ----------------------------------------------------------

	void  SetColorKey(DWORD ckey) { m_colorKey = ckey; m_hasColorKey = true; }
	bool  HasColorKey() const { return m_hasColorKey; }
	DWORD GetColorKey() const { return m_colorKey; }

	// --- Utility ------------------------------------------------------------

	void Fill(DWORD col);
	void Fill(DWORD x, DWORD y, DWORD w, DWORD h, DWORD col);

private:
	GLuint m_texId;         // resolve / sample texture
	GLuint m_fbo;           // single-sample FBO (texture attachment)
	GLuint m_msaaFbo;       // multisample FBO (renderbuffers) — 0 when m_samples==0
	GLuint m_msaaColorRbo;
	GLuint m_msaaDepthRbo;
	GLuint m_depthRbo;      // single-sample depth/stencil when no MSAA

	DWORD m_width;
	DWORD m_height;
	DWORD m_attrib;
	int   m_refCount;
	int   m_samples;
	bool  m_hasStencil;
	bool  m_ownsTexture;
	bool  m_fboFailed;      // set when EnsureFBO() has failed once; skip retries

	DWORD m_colorKey;
	bool  m_hasColorKey;

	// State saved by BindFBO/UnbindFBO so scene renders can nest RTs safely.
	GLint m_prevDrawFBO;
	GLint m_prevReadFBO;
	GLint m_prevViewport[4];
};

// --- Namespace helpers ------------------------------------------------------

// RAII framebuffer binder: saves draw+read FBO and viewport on construction,
// restores them on destruction. `fbo == 0` is valid (binds the default
// framebuffer / backbuffer). When `w==0 || h==0` the viewport is left
// untouched.
class FBOBinder {
public:
	FBOBinder(GLuint fbo, int w, int h);
	~FBOBinder();

	FBOBinder(const FBOBinder&)            = delete;
	FBOBinder& operator=(const FBOBinder&) = delete;

private:
	GLint m_prevDraw;
	GLint m_prevRead;
	GLint m_prevVp[4];
	bool  m_restoreVp;
};

} // namespace ogl

#endif // !_WIN32
#endif // __OGLSURFACE_H
