// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// OGLSketchpad — oapi::Sketchpad implementation for the OpenGL client.
//
// Renders every primitive directly into the bound SURFHANDLE's FBO via a
// dedicated 2D shader (shaders/sketchpad.*); the previous implementation
// drew through ImGui::GetForegroundDrawList() and so never touched the
// target surface, leaving MFD textures black.
//
// Text is drawn by emitting per-glyph textured quads that sample ImGui's
// font atlas (already uploaded to GL as part of ImGui_ImplOpenGL3 init).

#ifndef __OGLSKETCHPAD_H
#define __OGLSKETCHPAD_H

#ifndef _WIN32

#include "DrawAPI.h"
#include "OrbiterAPI.h"
#include <OpenGL/gl3.h>

struct ImFont;

namespace ogl {

class ShaderMgr;
class OGLSurface;

// --- Font / Pen / Brush wrappers ------------------------------------------

class OGLFont : public oapi::Font {
public:
	OGLFont(int height, bool prop, const char *face,
	        FontStyle style, int orientation);
	ImFont *imFont;
	float   fontSize;
};

class OGLPen : public oapi::Pen {
public:
	OGLPen(int style, int width, DWORD col);
	int   style;     // 0=none, 1=solid, 2=dashed
	int   width;     // pixel width, min 1
	DWORD col;       // 0xBBGGRR (Orbiter convention)
};

class OGLBrush : public oapi::Brush {
public:
	OGLBrush(DWORD col);
	DWORD col;
};

// --- Sketchpad -------------------------------------------------------------

class OGLSketchpad : public oapi::Sketchpad {
public:
	OGLSketchpad(SURFHANDLE surf, ShaderMgr *sm);
	~OGLSketchpad();

	// -- Font / Pen / Brush state --
	oapi::Font *SetFont(oapi::Font *font) override;
	oapi::Pen *SetPen(oapi::Pen *pen) override;
	oapi::Brush *SetBrush(oapi::Brush *brush) override;

	// -- Text state --
	void SetTextAlign(TAlign_horizontal tah = LEFT,
	                  TAlign_vertical   tav = TOP) override;
	DWORD SetTextColor(DWORD col) override;
	DWORD SetBackgroundColor(DWORD col) override;
	void  SetBackgroundMode(BkgMode mode) override;

	// -- Origin --
	void SetOrigin(int x, int y) override;
	void GetOrigin(int *x, int *y) const override;

	// -- Metrics --
	DWORD GetCharSize() override;
	DWORD GetTextWidth(const char *str, int len = 0) override;

	// -- Primitives --
	bool Text(int x, int y, const char *str, int len) override;
	void MoveTo(int x, int y) override;
	void LineTo(int x, int y) override;
	void Line(int x0, int y0, int x1, int y1) override;
	void Rectangle(int x0, int y0, int x1, int y1) override;
	void Ellipse(int x0, int y0, int x1, int y1) override;
	void Polygon(const oapi::IVECTOR2 *pt, int npt) override;
	void Polyline(const oapi::IVECTOR2 *pt, int npt) override;
	void Pixel(int x, int y, DWORD col) override;

	// -- Colour transforms --
	void      SetBrightness(const oapi::FVECTOR4 *pBrightness = nullptr) override;
	void      SetColorMatrix(const oapi::FMATRIX4 *pMatrix = nullptr) override;
	const oapi::FMATRIX4 *GetColorMatrix() override;
	void      SetRenderParam(RenderParam param,
	                         const oapi::FVECTOR4 *data = nullptr) override;
	oapi::FVECTOR4 GetRenderParam(RenderParam param) override;

	// -- Shared GL resource lifecycle --
	static void InitShared(ShaderMgr *sm);
	static void ReleaseShared();

private:
	// Render-mode constants for the shared fragment shader.
	enum Mode : int { MODE_SOLID = 0, MODE_TEXTURE = 1, MODE_TEXT = 2 };

	void BindState();
	void UnbindState();

	// Low-level draw helpers. All coordinates are already in target-pixel
	// space (no origin offset applied here — the public methods fold it in).
	void DrawSolidTriangles(const float *xy, int nVerts, DWORD col);
	void DrawSolidLines(const float *xy, int nVerts, DWORD col, int width);
	void DrawStrokedPolyline(const float *xy, int nVerts, DWORD col,
	                         int width, bool closed);
	void DrawTexturedQuads(const float *xyuv, int nVerts,
	                       GLuint tex, DWORD col, Mode mode);

	// Packs Orbiter's 0xBBGGRR (+ optional alpha) into a vec4 for the shader.
	static void PackColor(DWORD col, float out[4]);

	// Target surface state.
	OGLSurface *m_surf;
	ShaderMgr  *m_sm;
	DWORD       m_viewW, m_viewH;

	// Selected resources.
	OGLFont  *m_font;
	OGLPen   *m_pen;
	OGLBrush *m_brush;

	// Text state.
	DWORD              m_textCol;
	DWORD              m_bgCol;
	BkgMode            m_bkMode;
	TAlign_horizontal  m_tah;
	TAlign_vertical    m_tav;

	// Movable current point + origin offset.
	int m_cx, m_cy;
	int m_ox, m_oy;

	// Shared program + VAO/VBO pool (one VAO is enough — all draws stream
	// into the same dynamic VBO).
	static GLuint s_program;
	static GLuint s_vao;
	static GLuint s_vbo;
	static GLint  s_locViewport;
	static GLint  s_locColor;
	static GLint  s_locMode;
	static GLint  s_locTexture;
	static GLint  s_locBrightness;
	static GLint  s_locColorMat;
	static GLint  s_locGamma;
	static GLint  s_locNoise;
	static bool   s_sharedInitialized;

	// Colour-transform state. Defaults are identity so a Sketchpad that
	// never calls Set* paints the same as before M17.b.
	float m_brightness[4];  // multiplicative rgba
	float m_colorMat[16];   // row-major 4x4
	float m_gamma[4];       // rgb = 1/gamma exponent
	float m_noise[4];       // rgb tint, a blend (0 = off)

	// Shadow of the last SetColorMatrix call so GetColorMatrix() can
	// return a pointer with the same lifetime as the Sketchpad.
	oapi::FMATRIX4 m_colorMatShadow;
};

} // namespace ogl

#endif // !_WIN32
#endif // __OGLSKETCHPAD_H
