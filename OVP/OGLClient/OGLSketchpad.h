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
#include <array>
#include <vector>

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

	// -- Quick resource + pen/brush helpers --
	void QuickPen(DWORD color, float width = 1.0f, DWORD style = 1) override;
	void QuickBrush(DWORD color) override;
	void SetGlobalLineScale(float width = 1.0f, float pattern = 1.0f) override;
	void ColorCompatibility(bool bEnable) override;

	// -- State --
	void SetBlendState(BlendState state = (BlendState)(BlendState::ALPHABLEND |
	                                                   BlendState::FILTER_LINEAR)) override;
	void DepthEnable(bool enable) override;
	void Clear(DWORD color = 0, bool bColor = true, bool bDepth = true) override;

	// -- Fills --
	void ColorFill(DWORD color, const LPRECT tgt) override;
	void GradientFillRect(const LPRECT tgt, DWORD c1, DWORD c2,
	                      bool bVertical = false) override;
	void FillTetragon(DWORD color, const oapi::FVECTOR2 pt[4]) override;

	// -- Blits --
	void CopyRect(const SURFHANDLE hSrc, const LPRECT src, int tx, int ty) override;
	void StretchRect(const SURFHANDLE hSrc, const LPRECT src = nullptr,
	                 const LPRECT tgt = nullptr) override;
	void RotateRect(const SURFHANDLE hSrc, const LPRECT src,
	                int cx, int cy, float angle = 0.0f,
	                float sw = 1.0f, float sh = 1.0f) override;
	void ColorKey(const SURFHANDLE hSrc, const LPRECT src, int tx, int ty) override;
	void CopyTetragon(const SURFHANDLE hSrc, const LPRECT sr,
	                  const oapi::FVECTOR2 pt[4]) override;
	void StretchRegion(const skpRegion *rgn, const SURFHANDLE hSrc,
	                   const LPRECT out) override;

	// -- Misc primitives --
	void Lines(const oapi::FVECTOR2 *pt1, int nlines) override;
	bool TextW(int x, int y, const LPWSTR str, int len) override;
	void TextEx(float x, float y, const char *str,
	            float scale = 1.0f, float angle = 0.0f) override;

	// -- Transform state --
	void SetWorldTransform(const oapi::FMATRIX4 *pWT = nullptr) override;
	void SetWorldTransform2D(float scale = 1.0f, float rot = 0.0f,
	                         const oapi::IVECTOR2 *ctr = nullptr,
	                         const oapi::IVECTOR2 *trl = nullptr) override;
	oapi::FMATRIX4 GetWorldTransform() const override;
	void PushWorldTransform() override;
	void PopWorldTransform() override;

	// -- View / projection --
	void SetViewMatrix(const oapi::FMATRIX4 *pV = nullptr) override;
	void SetProjectionMatrix(const oapi::FMATRIX4 *pP = nullptr) override;
	const oapi::FMATRIX4 *ViewMatrix() const override;
	const oapi::FMATRIX4 *ProjectionMatrix() const override;
	const oapi::FMATRIX4 *GetViewProjectionMatrix() const override;
	void SetViewMode(SkpView mode = ORTHO) override;

	// -- Clipping / surface metrics --
	void ClipRect(const LPRECT pClip = nullptr) override;
	void Clipper(int idx, const VECTOR3 *pPos = nullptr,
	             double cos_angle = 0.0, double dist = 0.0) override;
	void SetClipDistance(float _near, float _far) override;
	void GetRenderSurfaceSize(LPSIZE size) override;

	// -- Mesh + world-facing primitives --
	int  DrawMeshGroup(const MESHHANDLE hMesh, DWORD grp,
	                   MeshFlags flags = MeshFlags::SMOOTH_SHADE,
	                   const SURFHANDLE hTex = nullptr) override;
	void DrawPoly(const HPOLY hPoly, DWORD flags = 0) override;
	void SetWorldBillboard(const oapi::FVECTOR3 &wpos, float scl = 1.0f,
	                       bool bFixed = true,
	                       const oapi::FVECTOR3 *index = nullptr) override;

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

	// Packs Orbiter's 0xBBGGRR (+ optional alpha) into a vec4 for the
	// shader. Honours m_colourCompat — when true (default) alpha==0 is
	// rewritten to 0xFF, matching historical D3D9 semantics.
	void PackColor(DWORD col, float out[4]) const;

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
	static GLint  s_locColor2;
	static GLint  s_locColorKey;
	static GLint  s_locWorld;
	static GLint  s_locViewProj;
	static GLint  s_locViewMode;
	static GLint  s_locClipperDir;
	static GLint  s_locClipperCosDist;
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

	// SetGlobalLineScale multipliers — applied to m_pen->width (and, in
	// the future, to dashed pattern repeats) when stroking primitives.
	float m_lineScale;
	float m_patternScale;

	// Transient Pen/Brush owned by QuickPen / QuickBrush — the public
	// API lets callers skip the create-set-release dance. Destroyed
	// when the Sketchpad is released.
	OGLPen   *m_quickPen;
	OGLBrush *m_quickBrush;

	// Colour compatibility toggle (ColorCompatibility). When true the
	// alpha=0 byte of every input colour is rewritten to 255 — matches
	// the legacy D3D9 "alpha 0 means opaque" default.
	bool m_colourCompat;

	// Transform + view/projection state. Identity by default so ORTHO
	// pixel-space mode reproduces pre-M17.d behaviour exactly.
	float m_world[16];
	float m_view[16];
	float m_proj[16];
	float m_viewProj[16];    // view * proj, recomputed on any Set*Matrix
	int   m_viewMode;        // ORTHO = 0, USER = 1

	// Shadow pointers for ViewMatrix()/ProjectionMatrix() getters whose
	// lifetime must outlive a single call.
	oapi::FMATRIX4 m_viewShadow, m_projShadow, m_viewProjShadow;

	// World-transform stack for PushWorldTransform/PopWorldTransform.
	// Small reserved capacity covers the usual one-two deep usage.
	std::vector<std::array<float, 16>> m_worldStack;

	// Two Clipper slots (world-space cone).
	float m_clipperDir[2][4];          // xyz = unit direction, w = enable
	float m_clipperCosDist[2][2];      // x = cos(angle), y = near-distance

	// Near/far clip planes for SetClipDistance. Only meaningful in USER
	// view mode — default values match the D3D9 defaults.
	float m_clipNear, m_clipFar;
};

} // namespace ogl

#endif // !_WIN32
#endif // __OGLSKETCHPAD_H
