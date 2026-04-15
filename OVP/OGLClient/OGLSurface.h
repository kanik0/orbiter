// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// OGLSurface - Surface wrapper for OGLClient
// Encapsulates a GL texture with optional FBO (render-target), color key,
// and reference counting. Replaces bare OGLTexture as the SURFHANDLE type.

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

	// --- Creation ---

	// Create an empty surface with given dimensions and attribute flags.
	// attrib is a combination of OAPISURFACE_* flags.
	bool Create(DWORD w, DWORD h, DWORD attrib);

	// Wrap an existing GL texture ID (takes ownership).
	// Used when loading textures from files via OGLTexture.
	void WrapTexture(GLuint texId, DWORD w, DWORD h);

	// --- Properties ---

	GLuint GetTexture() const { return m_texId; }
	DWORD  GetWidth() const { return m_width; }
	DWORD  GetHeight() const { return m_height; }
	DWORD  GetAttrib() const { return m_attrib; }
	bool   IsRenderTarget() const { return m_fbo != 0; }

	// --- Reference counting ---

	void   AddRef() { m_refCount++; }
	int    Release(); // returns new ref count; deletes self if 0
	int    GetRefCount() const { return m_refCount; }

	// --- FBO for render-to-texture ---

	// Ensure an FBO exists for this surface (lazy-created on first use).
	// Returns the FBO id, or 0 on failure.
	GLuint EnsureFBO();

	// Bind this surface's FBO as the current render target.
	// Saves the previous FBO binding so it can be restored with UnbindFBO().
	void   BindFBO();

	// Restore the previous FBO binding.
	void   UnbindFBO();

	// --- Color key ---

	void   SetColorKey(DWORD ckey) { m_colorKey = ckey; m_hasColorKey = true; }
	bool   HasColorKey() const { return m_hasColorKey; }
	DWORD  GetColorKey() const { return m_colorKey; }

	// --- Utility ---

	// Fill the entire surface with a solid color (ARGB).
	void   Fill(DWORD col);

	// Fill a rectangular region with a solid color.
	void   Fill(DWORD x, DWORD y, DWORD w, DWORD h, DWORD col);

private:
	GLuint m_texId;        // GL texture name
	GLuint m_fbo;          // Framebuffer object (0 = not a render target yet)
	GLuint m_depthRbo;     // Depth renderbuffer attachment for FBO
	DWORD  m_width;
	DWORD  m_height;
	DWORD  m_attrib;       // OAPISURFACE_* flags
	int    m_refCount;
	bool   m_ownsTexture;  // true if we should delete the texture on destruction

	DWORD  m_colorKey;
	bool   m_hasColorKey;

	GLint  m_prevFBO;      // saved FBO binding during BindFBO/UnbindFBO
};

} // namespace ogl

#endif // !_WIN32
#endif // __OGLSURFACE_H
