// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef _WIN32

#include "OGLSketchpad.h"
#include "OGLShaderMgr.h"
#include "OGLSurface.h"
#include "imgui.h"
#include "imgui_internal.h"   // ImTextCharFromUtf8
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <vector>

namespace ogl {

// --- Static shared state ---------------------------------------------------

GLuint OGLSketchpad::s_program = 0;
GLuint OGLSketchpad::s_vao     = 0;
GLuint OGLSketchpad::s_vbo     = 0;
GLint  OGLSketchpad::s_locViewport = -1;
GLint  OGLSketchpad::s_locColor    = -1;
GLint  OGLSketchpad::s_locMode     = -1;
GLint  OGLSketchpad::s_locTexture  = -1;
GLint  OGLSketchpad::s_locBrightness = -1;
GLint  OGLSketchpad::s_locColorMat   = -1;
GLint  OGLSketchpad::s_locGamma      = -1;
GLint  OGLSketchpad::s_locNoise      = -1;
GLint  OGLSketchpad::s_locColor2     = -1;
GLint  OGLSketchpad::s_locColorKey   = -1;
GLint  OGLSketchpad::s_locWorld      = -1;
GLint  OGLSketchpad::s_locViewProj   = -1;
GLint  OGLSketchpad::s_locViewMode   = -1;
GLint  OGLSketchpad::s_locClipperDir = -1;
GLint  OGLSketchpad::s_locClipperCosDist = -1;
GLuint OGLSketchpad::s_samplerPoint  = 0;
GLuint OGLSketchpad::s_samplerLinear = 0;
GLuint OGLSketchpad::s_samplerAniso  = 0;
bool   OGLSketchpad::s_sharedInitialized = false;

void OGLSketchpad::InitShared(ShaderMgr *sm)
{
	if (s_sharedInitialized) return;
	s_sharedInitialized = true;

	s_program = sm->LoadProgram("sketchpad", "sketchpad.vert", "sketchpad.frag");
	s_locViewport   = sm->GetUniformLoc(s_program, "uViewport");
	s_locColor      = sm->GetUniformLoc(s_program, "uColor");
	s_locMode       = sm->GetUniformLoc(s_program, "uMode");
	s_locTexture    = sm->GetUniformLoc(s_program, "uTexture");
	s_locBrightness = sm->GetUniformLoc(s_program, "uBrightness");
	s_locColorMat   = sm->GetUniformLoc(s_program, "uColorMat");
	s_locGamma      = sm->GetUniformLoc(s_program, "uGamma");
	s_locNoise      = sm->GetUniformLoc(s_program, "uNoise");
	s_locColor2     = sm->GetUniformLoc(s_program, "uColor2");
	s_locColorKey   = sm->GetUniformLoc(s_program, "uColorKey");
	s_locWorld      = sm->GetUniformLoc(s_program, "uWorld");
	s_locViewProj   = sm->GetUniformLoc(s_program, "uViewProj");
	s_locViewMode   = sm->GetUniformLoc(s_program, "uViewMode");
	s_locClipperDir     = sm->GetUniformLoc(s_program, "uClipperDir[0]");
	s_locClipperCosDist = sm->GetUniformLoc(s_program, "uClipperCosDist[0]");

	// One streaming VBO is enough — every Draw* call rewrites it before the
	// glDrawArrays, so there is no benefit from separate per-primitive VAOs.
	glGenVertexArrays(1, &s_vao);
	glGenBuffers(1, &s_vbo);
	glBindVertexArray(s_vao);
	glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
	glEnableVertexAttribArray(1);
	glBindVertexArray(0);

	// Sampler objects for BlendState::FILTER_* hints. Samplers override
	// the bound texture's min/mag filter params for the texture unit
	// they're attached to, so we can honour a per-blit filter request
	// without mutating the source surface's sampler state.
	GLuint samplers[3] = { 0, 0, 0 };
	glGenSamplers(3, samplers);
	s_samplerPoint  = samplers[0];
	s_samplerLinear = samplers[1];
	s_samplerAniso  = samplers[2];

	glSamplerParameteri(s_samplerPoint,  GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glSamplerParameteri(s_samplerPoint,  GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glSamplerParameteri(s_samplerLinear, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glSamplerParameteri(s_samplerLinear, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glSamplerParameteri(s_samplerAniso,  GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glSamplerParameteri(s_samplerAniso,  GL_TEXTURE_MAG_FILTER, GL_LINEAR);
#ifdef GL_TEXTURE_MAX_ANISOTROPY
	// EXT_texture_filter_anisotropic — widely available on desktop GL
	// but not a GL 4.1 core feature. Clamp to whatever the driver
	// actually supports instead of guessing at a 16× ceiling.
	GLfloat maxAniso = 1.0f;
	glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAniso);
	glSamplerParameterf(s_samplerAniso, GL_TEXTURE_MAX_ANISOTROPY, maxAniso);
#endif
}

void OGLSketchpad::ReleaseShared()
{
	if (!s_sharedInitialized) return;
	if (s_vao) { glDeleteVertexArrays(1, &s_vao); s_vao = 0; }
	if (s_vbo) { glDeleteBuffers(1, &s_vbo); s_vbo = 0; }
	GLuint samplers[3] = { s_samplerPoint, s_samplerLinear, s_samplerAniso };
	glDeleteSamplers(3, samplers);
	s_samplerPoint = s_samplerLinear = s_samplerAniso = 0;
	s_program = 0;
	s_sharedInitialized = false;
}

// --- Font / Pen / Brush ----------------------------------------------------

OGLFont::OGLFont(int height, bool prop, const char *face,
                 FontStyle style, int orientation)
	: oapi::Font(height, prop, face, style, orientation),
	  imFont(nullptr), fontSize((float)(height < 0 ? -height : height))
{
	// Pick from ImGui's built-in atlas. The loader registers three fonts:
	// index 0 = default proportional, index 2 = fixed-width (if available).
	// Until we ship our own .ttf pack the face name is only used to swap
	// between the two buckets.
	ImGuiIO &io = ImGui::GetIO();
	if (!io.Fonts || io.Fonts->Fonts.Size == 0) return;
	const bool wantFixed =
		!prop ||
		(face && (!strcmp(face, "Fixed") || !strcmp(face, "Courier") ||
		          !strcmp(face, "Courier New")));
	if (wantFixed && io.Fonts->Fonts.Size > 2)
		imFont = io.Fonts->Fonts[2];
	else
		imFont = io.Fonts->Fonts[0];
}

OGLPen::OGLPen(int s, int w, DWORD c)
	: oapi::Pen(s, w, c), style(s), width(w < 1 ? 1 : w), col(c) {}

OGLBrush::OGLBrush(DWORD c) : oapi::Brush(c), col(c) {}

// --- Sketchpad ctor/dtor ---------------------------------------------------

OGLSketchpad::OGLSketchpad(SURFHANDLE surf, ShaderMgr *sm)
	: oapi::Sketchpad(surf), m_surf((OGLSurface*)surf), m_sm(sm),
	  m_viewW(0), m_viewH(0),
	  m_font(nullptr), m_pen(nullptr), m_brush(nullptr),
	  m_textCol(0xFFFFFF), m_bgCol(0x000000),
	  m_bkMode(BK_TRANSPARENT), m_tah(LEFT), m_tav(TOP),
	  m_cx(0), m_cy(0), m_ox(0), m_oy(0),
	  m_lineScale(1.0f), m_patternScale(1.0f),
	  m_quickPen(nullptr), m_quickBrush(nullptr),
	  m_colourCompat(true)
{
	if (m_surf) {
		m_viewW = m_surf->GetWidth();
		m_viewH = m_surf->GetHeight();
	}

	// Identity colour transforms — no visible effect until SetBrightness /
	// SetColorMatrix / SetRenderParam installs non-default values.
	m_brightness[0] = m_brightness[1] = m_brightness[2] = m_brightness[3] = 1.0f;
	for (int i = 0; i < 16; i++) m_colorMat[i] = (i % 5 == 0) ? 1.0f : 0.0f;
	m_gamma[0] = m_gamma[1] = m_gamma[2] = 1.0f; m_gamma[3] = 0.0f;
	m_noise[0] = m_noise[1] = m_noise[2] = m_noise[3] = 0.0f;
	// Shadow starts as identity so a GetColorMatrix before any Set returns
	// a valid pointer rather than undefined memory.
	float *m = (float*)&m_colorMatShadow;
	for (int i = 0; i < 16; i++) m[i] = (i % 5 == 0) ? 1.0f : 0.0f;

	// Transform + view/proj state defaults to identity / ORTHO so pre-M17.d
	// callers see no behavioural change.
	for (int i = 0; i < 16; i++) {
		m_world[i]    = (i % 5 == 0) ? 1.0f : 0.0f;
		m_view[i]     = (i % 5 == 0) ? 1.0f : 0.0f;
		m_proj[i]     = (i % 5 == 0) ? 1.0f : 0.0f;
		m_viewProj[i] = (i % 5 == 0) ? 1.0f : 0.0f;
	}
	float *vs = (float*)&m_viewShadow;
	float *ps = (float*)&m_projShadow;
	float *vps = (float*)&m_viewProjShadow;
	for (int i = 0; i < 16; i++) {
		vs[i] = ps[i] = vps[i] = (i % 5 == 0) ? 1.0f : 0.0f;
	}
	m_viewMode = ORTHO;
	m_clipNear = 0.1f;
	m_clipFar  = 1000.0f;
	for (int i = 0; i < 2; i++) {
		m_clipperDir[i][0] = m_clipperDir[i][1] = m_clipperDir[i][2] = 0.0f;
		m_clipperDir[i][3] = 0.0f;   // disabled
		m_clipperCosDist[i][0] = 1.0f;
		m_clipperCosDist[i][1] = 0.0f;
	}

	BindState();
}

OGLSketchpad::~OGLSketchpad()
{
	UnbindState();
	delete m_quickPen;
	delete m_quickBrush;
}

void OGLSketchpad::BindState()
{
	if (!m_surf || !s_program) return;

	// OGLSurface::BindFBO already snapshots the draw/read FBO + viewport
	// internally and restores them on UnbindFBO. We still save the shader
	// program, VAO and blend/depth toggles because the caller (e.g. the
	// scene render path) may rely on those.
	m_surf->BindFBO();

	glUseProgram(s_program);
	glBindVertexArray(s_vao);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_SCISSOR_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glUniform2f(s_locViewport, (float)m_viewW, (float)m_viewH);
	glUniform4fv(s_locBrightness, 1, m_brightness);
	glUniformMatrix4fv(s_locColorMat, 1, GL_FALSE, m_colorMat);
	glUniform4fv(s_locGamma, 1, m_gamma);
	glUniform4fv(s_locNoise, 1, m_noise);

	// Colour-key + second colour default off / transparent — blit and
	// gradient helpers will overwrite them as needed per-draw.
	const float off[4] = { 0, 0, 0, 0 };
	glUniform4fv(s_locColor2, 1, off);
	glUniform4fv(s_locColorKey, 1, off);

	// Transform + projection state.
	glUniformMatrix4fv(s_locWorld,    1, GL_FALSE, m_world);
	glUniformMatrix4fv(s_locViewProj, 1, GL_FALSE, m_viewProj);
	glUniform1i       (s_locViewMode, m_viewMode);

	// Clipper slots default to disabled.
	float dir[8]     = { 0,0,0,0, 0,0,0,0 };
	float cosdist[4] = { 1,0, 1,0 };
	for (int i = 0; i < 2; i++) {
		dir[i*4 + 0] = m_clipperDir[i][0];
		dir[i*4 + 1] = m_clipperDir[i][1];
		dir[i*4 + 2] = m_clipperDir[i][2];
		dir[i*4 + 3] = m_clipperDir[i][3];
		cosdist[i*2 + 0] = m_clipperCosDist[i][0];
		cosdist[i*2 + 1] = m_clipperCosDist[i][1];
	}
	glUniform4fv(s_locClipperDir,     2, dir);
	glUniform2fv(s_locClipperCosDist, 2, cosdist);
}

void OGLSketchpad::UnbindState()
{
	if (!m_surf || !s_program) return;

	glBindVertexArray(0);
	glUseProgram(0);
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	m_surf->UnbindFBO();
}

// --- State setters ---------------------------------------------------------

oapi::Font *OGLSketchpad::SetFont(oapi::Font *font) {
	oapi::Font *prev = m_font; m_font = (OGLFont*)font; return prev;
}

oapi::Pen *OGLSketchpad::SetPen(oapi::Pen *pen) {
	oapi::Pen *prev = m_pen; m_pen = (OGLPen*)pen; return prev;
}

oapi::Brush *OGLSketchpad::SetBrush(oapi::Brush *brush) {
	oapi::Brush *prev = m_brush; m_brush = (OGLBrush*)brush; return prev;
}

void OGLSketchpad::SetTextAlign(TAlign_horizontal tah, TAlign_vertical tav) {
	m_tah = tah; m_tav = tav;
}

DWORD OGLSketchpad::SetTextColor(DWORD col) {
	DWORD prev = m_textCol; m_textCol = col; return prev;
}

DWORD OGLSketchpad::SetBackgroundColor(DWORD col) {
	DWORD prev = m_bgCol; m_bgCol = col; return prev;
}

void OGLSketchpad::SetBackgroundMode(BkgMode mode) { m_bkMode = mode; }

void OGLSketchpad::SetOrigin(int x, int y) { m_ox = x; m_oy = y; }

void OGLSketchpad::GetOrigin(int *x, int *y) const {
	if (x) *x = m_ox;
	if (y) *y = m_oy;
}

// --- Metrics ---------------------------------------------------------------

DWORD OGLSketchpad::GetCharSize() {
	// Orbiter packs (width << 16) | height. We measure against the ImGui
	// reference glyph so the returned width tracks the actual font.
	ImFont *imf = m_font ? m_font->imFont : ImGui::GetFont();
	float sz = m_font ? m_font->fontSize : 14.0f;
	if (!imf) return ((DWORD)8 << 16) | 14u;
	ImVec2 ref = imf->CalcTextSizeA(sz, FLT_MAX, 0, "M");
	DWORD h = (DWORD)std::max(1.0f, sz);
	DWORD w = (DWORD)std::max(1.0f, ref.x);
	return (w << 16) | h;
}

DWORD OGLSketchpad::GetTextWidth(const char *str, int len) {
	if (!str) return 0;
	ImFont *imf = m_font ? m_font->imFont : ImGui::GetFont();
	float sz = m_font ? m_font->fontSize : 14.0f;
	const char *end = (len > 0) ? str + len : str + std::strlen(str);
	ImVec2 tsz = imf->CalcTextSizeA(sz, FLT_MAX, 0, str, end);
	return (DWORD)std::max(0.0f, tsz.x);
}

// --- Draw helpers ----------------------------------------------------------

void OGLSketchpad::PackColor(DWORD col, float out[4]) const
{
	// Orbiter stores 0xBBGGRR. The alpha byte (bits 24-31) is optional;
	// the ColorCompatibility default treats alpha==0 as "fully opaque"
	// which is what every D3D9 caller historically assumed. Sketchpads
	// that opt out (ColorCompatibility(false)) get the raw alpha byte.
	float r = ((col >> 16) & 0xFF) / 255.0f;
	float g = ((col >>  8) & 0xFF) / 255.0f;
	float b = ( col        & 0xFF) / 255.0f;
	float a = ((col >> 24) & 0xFF) / 255.0f;
	if (m_colourCompat && a < 1.0f / 255.0f) a = 1.0f;
	out[0] = r; out[1] = g; out[2] = b; out[3] = a;
}

void OGLSketchpad::DrawSolidTriangles(const float *xy, int nVerts, DWORD col)
{
	if (!xy || nVerts < 3) return;
	std::vector<float> verts(nVerts * 4, 0.0f);
	for (int i = 0; i < nVerts; i++) {
		verts[i * 4 + 0] = xy[i * 2 + 0];
		verts[i * 4 + 1] = xy[i * 2 + 1];
	}
	float c[4]; PackColor(col, c);
	glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
	glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STREAM_DRAW);
	glUniform4fv(s_locColor, 1, c);
	glUniform1i(s_locMode, MODE_SOLID);
	glDrawArrays(GL_TRIANGLES, 0, nVerts);
}

void OGLSketchpad::DrawSolidLines(const float *xy, int nVerts, DWORD col, int width)
{
	if (!xy || nVerts < 2) return;
	std::vector<float> verts(nVerts * 4, 0.0f);
	for (int i = 0; i < nVerts; i++) {
		verts[i * 4 + 0] = xy[i * 2 + 0];
		verts[i * 4 + 1] = xy[i * 2 + 1];
	}
	float c[4]; PackColor(col, c);
	glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
	glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STREAM_DRAW);
	glUniform4fv(s_locColor, 1, c);
	glUniform1i(s_locMode, MODE_SOLID);
	glLineWidth((float)std::max(1, width));
	glDrawArrays(GL_LINES, 0, nVerts);
}

// Expand a polyline into a triangle strip so thick lines render correctly
// even where glLineWidth clamps at 1.0 (macOS Core Profile does). Each
// interior segment produces two triangles; joins use a plain miter at
// the segment midpoint, which matches D3D9Pad's "flat join" default and is
// visually indistinguishable at MFD-sized pen widths (≤ 3 px).
void OGLSketchpad::DrawStrokedPolyline(const float *xy, int nVerts, DWORD col,
                                       int width, bool closed)
{
	if (!xy || nVerts < 2) return;
	if (width <= 1) {
		// Fast path: just glLineWidth(1) line strip / loop.
		std::vector<float> verts(nVerts * 4, 0.0f);
		for (int i = 0; i < nVerts; i++) {
			verts[i * 4 + 0] = xy[i * 2 + 0];
			verts[i * 4 + 1] = xy[i * 2 + 1];
		}
		float c[4]; PackColor(col, c);
		glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
		glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STREAM_DRAW);
		glUniform4fv(s_locColor, 1, c);
		glUniform1i(s_locMode, MODE_SOLID);
		glLineWidth(1.0f);
		glDrawArrays(closed ? GL_LINE_LOOP : GL_LINE_STRIP, 0, nVerts);
		return;
	}

	// Thick path: emit two triangles per segment. Segment extents are
	// normalised and rotated 90° to give the perpendicular half-width.
	const float halfW = width * 0.5f;
	std::vector<float> tri;
	tri.reserve((nVerts + (closed ? 1 : 0)) * 6 * 4);

	const int nSeg = closed ? nVerts : nVerts - 1;
	for (int i = 0; i < nSeg; i++) {
		const float x0 = xy[(i * 2) + 0];
		const float y0 = xy[(i * 2) + 1];
		const int   j  = (i + 1) % nVerts;
		const float x1 = xy[(j * 2) + 0];
		const float y1 = xy[(j * 2) + 1];

		const float dx = x1 - x0, dy = y1 - y0;
		const float len = std::sqrt(dx * dx + dy * dy);
		if (len < 1e-5f) continue;
		const float nx = -dy / len * halfW;
		const float ny =  dx / len * halfW;

		// Quad corners: a0-a1-b1, a0-b1-b0.
		const float a0x = x0 + nx, a0y = y0 + ny;
		const float a1x = x0 - nx, a1y = y0 - ny;
		const float b0x = x1 + nx, b0y = y1 + ny;
		const float b1x = x1 - nx, b1y = y1 - ny;

		tri.insert(tri.end(), { a0x, a0y, 0, 0, a1x, a1y, 0, 0, b1x, b1y, 0, 0,
		                        a0x, a0y, 0, 0, b1x, b1y, 0, 0, b0x, b0y, 0, 0 });
	}
	if (tri.empty()) return;

	float c[4]; PackColor(col, c);
	glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
	glBufferData(GL_ARRAY_BUFFER, tri.size() * sizeof(float), tri.data(), GL_STREAM_DRAW);
	glUniform4fv(s_locColor, 1, c);
	glUniform1i(s_locMode, MODE_SOLID);
	glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(tri.size() / 4));
}

void OGLSketchpad::DrawTexturedQuads(const float *xyuv, int nVerts,
                                     GLuint tex, DWORD col, Mode mode)
{
	if (!xyuv || nVerts < 3 || !tex) return;
	float c[4]; PackColor(col, c);
	glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
	glBufferData(GL_ARRAY_BUFFER, nVerts * 4 * sizeof(float), xyuv, GL_STREAM_DRAW);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex);

	// Apply the caller's SetBlendState filter hint via a sampler object.
	// FILTER_LINEAR (0x00) is the default so the legacy behaviour
	// survives callers that never touch BlendState.
	GLuint sampler = s_samplerLinear;
	if      (m_filterBits == FILTER_POINT)       sampler = s_samplerPoint;
	else if (m_filterBits == FILTER_ANISOTROPIC) sampler = s_samplerAniso;
	glBindSampler(0, sampler);

	glUniform1i(s_locTexture, 0);
	glUniform4fv(s_locColor, 1, c);
	glUniform1i(s_locMode, mode);
	glDrawArrays(GL_TRIANGLES, 0, nVerts);

	// Unbind so unrelated downstream passes that sample unit 0
	// aren't surprised by our filter choice.
	glBindSampler(0, 0);
}

// --- Primitives ------------------------------------------------------------

bool OGLSketchpad::Text(int x, int y, const char *str, int len)
{
	if (!str || !s_program) return false;
	ImFont *imf = m_font ? m_font->imFont : ImGui::GetFont();
	float   sz  = m_font ? m_font->fontSize : 14.0f;
	if (!imf) return false;

	const char *end = (len > 0) ? str + len : str + std::strlen(str);
	ImVec2 tsz = imf->CalcTextSizeA(sz, FLT_MAX, 0, str, end);

	float px = (float)(x + m_ox);
	float py = (float)(y + m_oy);
	if (m_tah == CENTER) px -= tsz.x * 0.5f;
	else if (m_tah == RIGHT) px -= tsz.x;
	if (m_tav == BASELINE) py -= sz * 0.8f;
	else if (m_tav == BOTTOM) py -= tsz.y;

	if (m_bkMode == BK_OPAQUE) {
		const float r[2 * 3 * 2] = {
			px,         py,
			px + tsz.x, py,
			px,         py + tsz.y,
			px + tsz.x, py,
			px + tsz.x, py + tsz.y,
			px,         py + tsz.y,
		};
		DrawSolidTriangles(r, 6, m_bgCol);
	}

	// ImGui 1.92 bakes glyph metrics per requested size; we fetch the baked
	// entry for `sz` so glyph->X0..AdvanceX come back already scaled and do
	// not need a per-glyph multiply at draw time. The atlas texture id is
	// exposed via ImFontAtlas::TexRef; nullptr IDs happen transiently while
	// a dynamic atlas rebuilds.
	ImFontAtlas   *atlas  = ImGui::GetIO().Fonts;
	GLuint         atlasTex = atlas ? (GLuint)(std::uintptr_t)atlas->TexRef.GetTexID() : 0;
	ImFontBaked   *baked  = imf->GetFontBaked(sz);
	if (!atlasTex || !baked) return true;

	float pen = px;
	std::vector<float> quads;
	quads.reserve((end - str) * 6 * 4);

	for (const char *p = str; p < end; ) {
		unsigned int codepoint = 0;
		int bytes = ImTextCharFromUtf8(&codepoint, p, end);
		if (bytes <= 0) break;
		p += bytes;

		ImFontGlyph *glyph = baked->FindGlyph((ImWchar)codepoint);
		if (!glyph) continue;

		// The cached glyph->U0..V1 are only valid for the TexRef that
		// was current when the glyph was last baked. ImGui 1.92 may
		// repack the atlas at any time (new font-size triples arrive,
		// dynamic fit), invalidating cached UVs on glyphs baked during
		// earlier passes — those glyphs then sample empty texels. The
		// imgui.h comment is explicit: "Always use latest values from
		// GetCustomRect()." (#123)
		ImFontAtlasRect rect;
		float u0 = glyph->U0, u1 = glyph->U1, v0 = glyph->V0, v1 = glyph->V1;
		if (glyph->PackId >= 0 &&
		    atlas->GetCustomRect(glyph->PackId, &rect)) {
			u0 = rect.uv0.x; v0 = rect.uv0.y;
			u1 = rect.uv1.x; v1 = rect.uv1.y;
		}

		float gx0 = pen + glyph->X0;
		float gy0 = py  + glyph->Y0;
		float gx1 = pen + glyph->X1;
		float gy1 = py  + glyph->Y1;

		quads.insert(quads.end(), {
			gx0, gy0, u0, v0,
			gx1, gy0, u1, v0,
			gx0, gy1, u0, v1,
			gx1, gy0, u1, v0,
			gx1, gy1, u1, v1,
			gx0, gy1, u0, v1,
		});
		pen += glyph->AdvanceX;
	}

	if (!quads.empty()) {
		DrawTexturedQuads(quads.data(), (int)(quads.size() / 4),
		                  atlasTex, m_textCol, MODE_TEXT);
	}
	return true;
}

void OGLSketchpad::MoveTo(int x, int y) { m_cx = x + m_ox; m_cy = y + m_oy; }

void OGLSketchpad::LineTo(int x, int y)
{
	if (!m_pen || m_pen->style == 0) { m_cx = x + m_ox; m_cy = y + m_oy; return; }
	const int nx = x + m_ox, ny = y + m_oy;
	const float line[4] = { (float)m_cx, (float)m_cy, (float)nx, (float)ny };
	DrawStrokedPolyline(line, 2, m_pen->col, m_pen->width, false);
	m_cx = nx; m_cy = ny;
}

void OGLSketchpad::Line(int x0, int y0, int x1, int y1)
{
	if (!m_pen || m_pen->style == 0) return;
	const float line[4] = {
		(float)(x0 + m_ox), (float)(y0 + m_oy),
		(float)(x1 + m_ox), (float)(y1 + m_oy),
	};
	DrawStrokedPolyline(line, 2, m_pen->col, m_pen->width, false);
}

void OGLSketchpad::Rectangle(int x0, int y0, int x1, int y1)
{
	if (x0 == x1 || y0 == y1) return;
	float ax = (float)(x0 + m_ox), ay = (float)(y0 + m_oy);
	float bx = (float)(x1 + m_ox), by = (float)(y1 + m_oy);
	if (ax > bx) std::swap(ax, bx);
	if (ay > by) std::swap(ay, by);

	if (m_brush) {
		const float tri[6 * 2] = {
			ax, ay, bx, ay, ax, by,
			bx, ay, bx, by, ax, by,
		};
		DrawSolidTriangles(tri, 6, m_brush->col);
	}
	if (m_pen && m_pen->style != 0) {
		const float loop[4 * 2] = { ax, ay, bx, ay, bx, by, ax, by };
		DrawStrokedPolyline(loop, 4, m_pen->col, m_pen->width, true);
	}
}

void OGLSketchpad::Ellipse(int x0, int y0, int x1, int y1)
{
	if (x0 == x1 || y0 == y1) return;
	float ax = (float)(x0 + m_ox), ay = (float)(y0 + m_oy);
	float bx = (float)(x1 + m_ox), by = (float)(y1 + m_oy);
	if (ax > bx) std::swap(ax, bx);
	if (ay > by) std::swap(ay, by);
	const float cx = (ax + bx) * 0.5f;
	const float cy = (ay + by) * 0.5f;
	const float rx = (bx - ax) * 0.5f;
	const float ry = (by - ay) * 0.5f;

	// Pick segment count from the larger radius so circles and extreme
	// ellipses both look smooth without over-tesselating small glyphs.
	const float rMax = std::max(rx, ry);
	int nSeg = (int)std::max(8.0f, std::min(96.0f, 8.0f + rMax * 0.6f));

	std::vector<float> pts(nSeg * 2);
	for (int i = 0; i < nSeg; i++) {
		float a = (float)i / (float)nSeg * 2.0f * (float)M_PI;
		pts[i * 2 + 0] = cx + std::cos(a) * rx;
		pts[i * 2 + 1] = cy + std::sin(a) * ry;
	}

	if (m_brush) {
		// Triangle fan around the centre.
		std::vector<float> tri;
		tri.reserve(nSeg * 3 * 2);
		for (int i = 0; i < nSeg; i++) {
			int j = (i + 1) % nSeg;
			tri.insert(tri.end(), {
				cx, cy,
				pts[i * 2 + 0], pts[i * 2 + 1],
				pts[j * 2 + 0], pts[j * 2 + 1],
			});
		}
		DrawSolidTriangles(tri.data(), (int)(tri.size() / 2), m_brush->col);
	}
	if (m_pen && m_pen->style != 0) {
		DrawStrokedPolyline(pts.data(), nSeg, m_pen->col, m_pen->width, true);
	}
}

void OGLSketchpad::Polygon(const oapi::IVECTOR2 *pt, int npt)
{
	if (!pt || npt < 3) return;
	std::vector<float> pts(npt * 2);
	for (int i = 0; i < npt; i++) {
		pts[i * 2 + 0] = (float)(pt[i].x + m_ox);
		pts[i * 2 + 1] = (float)(pt[i].y + m_oy);
	}
	if (m_brush) {
		// Fan from vertex 0. Works for convex inputs; Orbiter's MFD code
		// uses Polygon for strictly convex glyph fills so this is safe.
		std::vector<float> tri;
		tri.reserve((npt - 2) * 3 * 2);
		for (int i = 1; i < npt - 1; i++) {
			tri.insert(tri.end(), {
				pts[0], pts[1],
				pts[i * 2 + 0], pts[i * 2 + 1],
				pts[(i + 1) * 2 + 0], pts[(i + 1) * 2 + 1],
			});
		}
		if (!tri.empty())
			DrawSolidTriangles(tri.data(), (int)(tri.size() / 2), m_brush->col);
	}
	if (m_pen && m_pen->style != 0) {
		DrawStrokedPolyline(pts.data(), npt, m_pen->col, m_pen->width, true);
	}
}

void OGLSketchpad::Polyline(const oapi::IVECTOR2 *pt, int npt)
{
	if (!pt || npt < 2 || !m_pen || m_pen->style == 0) return;
	std::vector<float> pts(npt * 2);
	for (int i = 0; i < npt; i++) {
		pts[i * 2 + 0] = (float)(pt[i].x + m_ox);
		pts[i * 2 + 1] = (float)(pt[i].y + m_oy);
	}
	DrawStrokedPolyline(pts.data(), npt, m_pen->col, m_pen->width, false);
}

void OGLSketchpad::Pixel(int x, int y, DWORD col)
{
	const float px = (float)(x + m_ox), py = (float)(y + m_oy);
	const float tri[6 * 2] = {
		px,        py,
		px + 1.0f, py,
		px,        py + 1.0f,
		px + 1.0f, py,
		px + 1.0f, py + 1.0f,
		px,        py + 1.0f,
	};
	DrawSolidTriangles(tri, 6, col);
}

// --- Colour transforms -----------------------------------------------------

void OGLSketchpad::SetBrightness(const oapi::FVECTOR4 *pBrightness)
{
	if (pBrightness) {
		m_brightness[0] = pBrightness->x;
		m_brightness[1] = pBrightness->y;
		m_brightness[2] = pBrightness->z;
		m_brightness[3] = pBrightness->w;
	} else {
		m_brightness[0] = m_brightness[1] = m_brightness[2] = m_brightness[3] = 1.0f;
	}
	if (s_locBrightness >= 0)
		glUniform4fv(s_locBrightness, 1, m_brightness);
}

void OGLSketchpad::SetColorMatrix(const oapi::FMATRIX4 *pMatrix)
{
	if (pMatrix) {
		std::memcpy(m_colorMat, pMatrix, sizeof(m_colorMat));
		m_colorMatShadow = *pMatrix;
	} else {
		for (int i = 0; i < 16; i++) m_colorMat[i] = (i % 5 == 0) ? 1.0f : 0.0f;
		float *m = (float*)&m_colorMatShadow;
		for (int i = 0; i < 16; i++) m[i] = (i % 5 == 0) ? 1.0f : 0.0f;
	}
	if (s_locColorMat >= 0)
		glUniformMatrix4fv(s_locColorMat, 1, GL_FALSE, m_colorMat);
}

const oapi::FMATRIX4 *OGLSketchpad::GetColorMatrix()
{
	return &m_colorMatShadow;
}

void OGLSketchpad::SetRenderParam(RenderParam param, const oapi::FVECTOR4 *data)
{
	// The oapi::FVECTOR4 layout matches GLSL's vec4; we can forward the
	// scalars straight through. `data == nullptr` resets that parameter's
	// effect to the identity.
	switch (param) {
	case PRM_GAMMA: {
		if (data) {
			// Orbiter passes the gamma exponent in .rgb; shader applies
			// pow(color, 1/gamma). Invert here so the shader does a plain
			// pow() per fragment.
			m_gamma[0] = data->x > 1e-6f ? 1.0f / data->x : 1.0f;
			m_gamma[1] = data->y > 1e-6f ? 1.0f / data->y : 1.0f;
			m_gamma[2] = data->z > 1e-6f ? 1.0f / data->z : 1.0f;
			m_gamma[3] = 0.0f;
		} else {
			m_gamma[0] = m_gamma[1] = m_gamma[2] = 1.0f; m_gamma[3] = 0.0f;
		}
		if (s_locGamma >= 0)
			glUniform4fv(s_locGamma, 1, m_gamma);
		break;
	}
	case PRM_NOISE: {
		if (data) {
			m_noise[0] = data->x;
			m_noise[1] = data->y;
			m_noise[2] = data->z;
			m_noise[3] = data->w;
		} else {
			m_noise[0] = m_noise[1] = m_noise[2] = m_noise[3] = 0.0f;
		}
		if (s_locNoise >= 0)
			glUniform4fv(s_locNoise, 1, m_noise);
		break;
	}
	}
}

oapi::FVECTOR4 OGLSketchpad::GetRenderParam(RenderParam param)
{
	switch (param) {
	case PRM_GAMMA: {
		// Shader stores 1/gamma; invert back for the caller.
		float gx = m_gamma[0] > 1e-6f ? 1.0f / m_gamma[0] : 1.0f;
		float gy = m_gamma[1] > 1e-6f ? 1.0f / m_gamma[1] : 1.0f;
		float gz = m_gamma[2] > 1e-6f ? 1.0f / m_gamma[2] : 1.0f;
		return oapi::FVECTOR4(gx, gy, gz, 0.0f);
	}
	case PRM_NOISE:
		return oapi::FVECTOR4(m_noise[0], m_noise[1], m_noise[2], m_noise[3]);
	}
	return oapi::FVECTOR4(0, 0, 0, 0);
}

// --- Quick resources -------------------------------------------------------

void OGLSketchpad::QuickPen(DWORD color, float width, DWORD style)
{
	delete m_quickPen;
	m_quickPen = new OGLPen((int)style, (int)(width + 0.5f), color);
	m_pen = m_quickPen;
}

void OGLSketchpad::QuickBrush(DWORD color)
{
	delete m_quickBrush;
	m_quickBrush = new OGLBrush(color);
	m_brush = m_quickBrush;
}

void OGLSketchpad::SetGlobalLineScale(float width, float pattern)
{
	m_lineScale    = width;
	m_patternScale = pattern;
}

void OGLSketchpad::ColorCompatibility(bool bEnable)
{
	m_colourCompat = bEnable;
}

// --- State -----------------------------------------------------------------

void OGLSketchpad::SetBlendState(BlendState state)
{
	// Low 4 bits pick the colour-combining rule; next nibble picks the
	// texture sampling filter hint. The filter selection is applied at
	// DrawTexturedQuads time via a sampler object, so the caller's
	// choice (POINT for nearest, LINEAR for bilinear, ANISOTROPIC for
	// max-aniso where supported) is honoured per blit without mutating
	// the source surface's own sampler.
	const int combiner = state & 0x0F;
	m_filterBits       = state & 0xF0;
	glEnable(GL_BLEND);
	switch (combiner) {
	case COPY:
		glBlendFuncSeparate(GL_ONE, GL_ZERO, GL_ONE, GL_ZERO);
		break;
	case COPY_ALPHA:
		glBlendFuncSeparate(GL_ZERO, GL_ONE, GL_ONE, GL_ZERO);
		break;
	case COPY_COLOR:
		glBlendFuncSeparate(GL_ONE, GL_ZERO, GL_ZERO, GL_ONE);
		break;
	case ALPHABLEND:
	default:
		glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
		                    GL_ONE,       GL_ONE_MINUS_SRC_ALPHA);
		break;
	}
}

void OGLSketchpad::DepthEnable(bool bEnable)
{
	if (bEnable) glEnable(GL_DEPTH_TEST);
	else         glDisable(GL_DEPTH_TEST);
}

void OGLSketchpad::Clear(DWORD color, bool bColor, bool bDepth)
{
	GLbitfield mask = 0;
	if (bColor) {
		float c[4]; PackColor(color, c);
		glClearColor(c[0], c[1], c[2], c[3]);
		mask |= GL_COLOR_BUFFER_BIT;
	}
	if (bDepth) {
		glClearDepth(1.0);
		mask |= GL_DEPTH_BUFFER_BIT;
	}
	if (mask) glClear(mask);
}

// --- Solid fills ----------------------------------------------------------

void OGLSketchpad::ColorFill(DWORD color, const LPRECT tgt)
{
	if (!tgt) return;
	float ax = (float)tgt->left, ay = (float)tgt->top;
	float bx = (float)tgt->right, by = (float)tgt->bottom;
	const float tri[6 * 2] = {
		ax, ay, bx, ay, ax, by,
		bx, ay, bx, by, ax, by,
	};
	DrawSolidTriangles(tri, 6, color);
}

void OGLSketchpad::GradientFillRect(const LPRECT tgt, DWORD c1, DWORD c2, bool bVertical)
{
	if (!tgt) return;
	float ax = (float)tgt->left, ay = (float)tgt->top;
	float bx = (float)tgt->right, by = (float)tgt->bottom;

	// Pack pos + interpolation t into the UV channel so the existing
	// pos/UV VBO layout can carry the gradient direction.
	const float verts[6 * 4] = {
		ax, ay, 0, 0,
		bx, ay, 1, 0,
		ax, by, 0, 1,
		bx, ay, 1, 0,
		bx, by, 1, 1,
		ax, by, 0, 1,
	};

	float ca[4], cb[4];
	PackColor(c1, ca); PackColor(c2, cb);
	glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STREAM_DRAW);
	glUniform4fv(s_locColor, 1, ca);
	glUniform4fv(s_locColor2, 1, cb);
	glUniform1i(s_locMode, bVertical ? 4 : 3);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	// Restore default Color2 so subsequent solid fills don't see stale data.
	const float off[4] = { 0, 0, 0, 0 };
	glUniform4fv(s_locColor2, 1, off);
}

void OGLSketchpad::FillTetragon(DWORD color, const oapi::FVECTOR2 pt[4])
{
	if (!pt) return;
	const float tri[6 * 2] = {
		pt[0].x, pt[0].y, pt[1].x, pt[1].y, pt[2].x, pt[2].y,
		pt[0].x, pt[0].y, pt[2].x, pt[2].y, pt[3].x, pt[3].y,
	};
	DrawSolidTriangles(tri, 6, color);
}

// --- Blit family ----------------------------------------------------------

static inline void UvRect(OGLSurface *surf, const LPRECT src,
                          float &u0, float &v0, float &u1, float &v1)
{
	const float sw = (float)surf->GetWidth();
	const float sh = (float)surf->GetHeight();
	if (src) {
		u0 = src->left   / sw;
		v0 = src->top    / sh;
		u1 = src->right  / sw;
		v1 = src->bottom / sh;
	} else {
		u0 = 0.0f; v0 = 0.0f; u1 = 1.0f; v1 = 1.0f;
	}
}

void OGLSketchpad::CopyRect(const SURFHANDLE hSrc, const LPRECT src, int tx, int ty)
{
	if (!hSrc) return;
	OGLSurface *srcS = (OGLSurface*)hSrc;

	float u0, v0, u1, v1;
	UvRect(srcS, src, u0, v0, u1, v1);
	// The public contract (DrawAPI.h:1499) lets callers mirror the image
	// with a negative-width or negative-height source rect. UvRect picks
	// up the sign naturally (u0 > u1 flips the sample), but the target
	// size must stay positive so the quad is still wound CCW.
	int w = src ? std::abs(src->right - src->left) : (int)srcS->GetWidth();
	int h = src ? std::abs(src->bottom - src->top) : (int)srcS->GetHeight();
	float ax = (float)tx, ay = (float)ty;
	float bx = ax + (float)w, by = ay + (float)h;

	const float verts[6 * 4] = {
		ax, ay, u0, v0,
		bx, ay, u1, v0,
		ax, by, u0, v1,
		bx, ay, u1, v0,
		bx, by, u1, v1,
		ax, by, u0, v1,
	};
	const float white = 1.0f;
	const float col[4] = { white, white, white, white };
	DrawTexturedQuads(verts, 6, srcS->GetTexture(), 0xFFFFFFFF, MODE_TEXTURE);
	(void)col;
}

void OGLSketchpad::StretchRect(const SURFHANDLE hSrc, const LPRECT src, const LPRECT tgt)
{
	if (!hSrc) return;
	OGLSurface *srcS = (OGLSurface*)hSrc;

	float u0, v0, u1, v1;
	UvRect(srcS, src, u0, v0, u1, v1);
	float ax, ay, bx, by;
	if (tgt) {
		ax = (float)tgt->left;  ay = (float)tgt->top;
		bx = (float)tgt->right; by = (float)tgt->bottom;
	} else {
		ax = 0.0f; ay = 0.0f;
		bx = (float)m_viewW; by = (float)m_viewH;
	}

	const float verts[6 * 4] = {
		ax, ay, u0, v0,
		bx, ay, u1, v0,
		ax, by, u0, v1,
		bx, ay, u1, v0,
		bx, by, u1, v1,
		ax, by, u0, v1,
	};
	DrawTexturedQuads(verts, 6, srcS->GetTexture(), 0xFFFFFFFF, MODE_TEXTURE);
}

void OGLSketchpad::RotateRect(const SURFHANDLE hSrc, const LPRECT src,
                              int cx, int cy, float angle, float sw, float sh)
{
	if (!hSrc) return;
	OGLSurface *srcS = (OGLSurface*)hSrc;

	float u0, v0, u1, v1;
	UvRect(srcS, src, u0, v0, u1, v1);
	// Mirror via negative-width src rect (DrawAPI.h:1522): UvRect already
	// swaps the UVs; absolute value here keeps the target quad shape.
	int rw = src ? std::abs(src->right - src->left) : (int)srcS->GetWidth();
	int rh = src ? std::abs(src->bottom - src->top) : (int)srcS->GetHeight();
	float hx = 0.5f * rw * sw;
	float hy = 0.5f * rh * sh;

	// Corners around the local origin, rotated by `angle` radians,
	// then translated to (cx, cy).
	const float c = std::cos(angle), s = std::sin(angle);
	auto rot = [&](float lx, float ly, float &ox, float &oy) {
		ox = cx + c * lx - s * ly;
		oy = cy + s * lx + c * ly;
	};
	float x0, y0, x1, y1, x2, y2, x3, y3;
	rot(-hx, -hy, x0, y0);
	rot( hx, -hy, x1, y1);
	rot( hx,  hy, x2, y2);
	rot(-hx,  hy, x3, y3);

	const float verts[6 * 4] = {
		x0, y0, u0, v0,
		x1, y1, u1, v0,
		x3, y3, u0, v1,
		x1, y1, u1, v0,
		x2, y2, u1, v1,
		x3, y3, u0, v1,
	};
	DrawTexturedQuads(verts, 6, srcS->GetTexture(), 0xFFFFFFFF, MODE_TEXTURE);
}

void OGLSketchpad::ColorKey(const SURFHANDLE hSrc, const LPRECT src, int tx, int ty)
{
	if (!hSrc) return;
	OGLSurface *srcS = (OGLSurface*)hSrc;
	if (!srcS->HasColorKey()) { CopyRect(hSrc, src, tx, ty); return; }

	// Activate the chroma-key discard branch by setting uColorKey.a = 1
	// with the source surface's key colour.
	DWORD key = srcS->GetColorKey();
	float k[4] = {
		((key >> 16) & 0xFF) / 255.0f,
		((key >>  8) & 0xFF) / 255.0f,
		( key        & 0xFF) / 255.0f,
		1.0f
	};
	glUniform4fv(s_locColorKey, 1, k);

	CopyRect(hSrc, src, tx, ty);

	// Restore default so the next unrelated draw doesn't discard.
	const float off[4] = { 0, 0, 0, 0 };
	glUniform4fv(s_locColorKey, 1, off);
}

void OGLSketchpad::CopyTetragon(const SURFHANDLE hSrc, const LPRECT sr,
                                const oapi::FVECTOR2 pt[4])
{
	if (!hSrc || !pt) return;
	OGLSurface *srcS = (OGLSurface*)hSrc;

	float u0, v0, u1, v1;
	UvRect(srcS, sr, u0, v0, u1, v1);

	const float verts[6 * 4] = {
		pt[0].x, pt[0].y, u0, v0,
		pt[1].x, pt[1].y, u1, v0,
		pt[3].x, pt[3].y, u0, v1,
		pt[1].x, pt[1].y, u1, v0,
		pt[2].x, pt[2].y, u1, v1,
		pt[3].x, pt[3].y, u0, v1,
	};
	DrawTexturedQuads(verts, 6, srcS->GetTexture(), 0xFFFFFFFF, MODE_TEXTURE);
}

// Nine-slice stretch. `rgn->intr` is the inner (fixed-extent) rect in
// source space; `rgn->outr` is the outer source rect. `out` is the target
// rect: corners keep their source size, edges stretch along one axis,
// centre stretches on both. Nine separate blits — the overhead is
// negligible since 9-slice is almost exclusively used by UI plugins at
// low call frequency.
void OGLSketchpad::StretchRegion(const skpRegion *rgn, const SURFHANDLE hSrc,
                                 const LPRECT out)
{
	if (!rgn || !hSrc || !out) return;

	const LONG sl = rgn->outr.left,  st = rgn->outr.top;
	const LONG sr = rgn->outr.right, sb = rgn->outr.bottom;
	const LONG il = rgn->intr.left,  it = rgn->intr.top;
	const LONG ir = rgn->intr.right, ib = rgn->intr.bottom;

	const LONG tl = out->left,  tt = out->top;
	const LONG tr = out->right, tb = out->bottom;
	const LONG tiL = tl + (il - sl);      // inner target rect, derived
	const LONG tiT = tt + (it - st);
	const LONG tiR = tr - (sr - ir);
	const LONG tiB = tb - (sb - ib);

	auto stretch = [&](LONG sx0, LONG sy0, LONG sx1, LONG sy1,
	                   LONG tx0, LONG ty0, LONG tx1, LONG ty1) {
		RECT s = { sx0, sy0, sx1, sy1 };
		RECT t = { tx0, ty0, tx1, ty1 };
		StretchRect(hSrc, &s, &t);
	};

	// corners
	stretch(sl, st, il, it, tl,  tt,  tiL, tiT);
	stretch(ir, st, sr, it, tiR, tt,  tr,  tiT);
	stretch(sl, ib, il, sb, tl,  tiB, tiL, tb);
	stretch(ir, ib, sr, sb, tiR, tiB, tr,  tb);
	// edges
	stretch(il, st, ir, it, tiL, tt,  tiR, tiT);
	stretch(il, ib, ir, sb, tiL, tiB, tiR, tb);
	stretch(sl, it, il, ib, tl,  tiT, tiL, tiB);
	stretch(ir, it, sr, ib, tiR, tiT, tr,  tiB);
	// centre
	stretch(il, it, ir, ib, tiL, tiT, tiR, tiB);
}

// --- Transform state ------------------------------------------------------

static inline void MakeIdentity(float m[16])
{
	for (int i = 0; i < 16; i++) m[i] = (i % 5 == 0) ? 1.0f : 0.0f;
}

static inline void Mul4x4(const float a[16], const float b[16], float out[16])
{
	// Column-major 4x4 multiply (same layout oapi::FMATRIX4 uses on disk).
	float tmp[16];
	for (int c = 0; c < 4; c++)
		for (int r = 0; r < 4; r++) {
			float s = 0.0f;
			for (int k = 0; k < 4; k++) s += a[k * 4 + r] * b[c * 4 + k];
			tmp[c * 4 + r] = s;
		}
	std::memcpy(out, tmp, sizeof(tmp));
}

void OGLSketchpad::SetWorldTransform(const oapi::FMATRIX4 *pWT)
{
	if (pWT) std::memcpy(m_world, pWT, sizeof(m_world));
	else     MakeIdentity(m_world);
	if (s_locWorld >= 0)
		glUniformMatrix4fv(s_locWorld, 1, GL_FALSE, m_world);
}

void OGLSketchpad::SetWorldTransform2D(float scale, float rot,
                                       const oapi::IVECTOR2 *ctr,
                                       const oapi::IVECTOR2 *trl)
{
	// Compose T(translation) * T(centre) * R(rot) * S(scale) * T(-centre).
	// Matches the D3D9Pad2 reference so 2D panel callers that mix
	// SetWorldTransform2D and SetOrigin see identical geometry.
	const float cx = ctr ? (float)ctr->x : 0.0f;
	const float cy = ctr ? (float)ctr->y : 0.0f;
	const float tx = trl ? (float)trl->x : 0.0f;
	const float ty = trl ? (float)trl->y : 0.0f;
	const float c  = std::cos(rot);
	const float s  = std::sin(rot);

	float m[16];
	MakeIdentity(m);
	// column-major 4x4: composite 2D affine in the xy plane.
	m[0 * 4 + 0] =  c * scale;
	m[0 * 4 + 1] =  s * scale;
	m[1 * 4 + 0] = -s * scale;
	m[1 * 4 + 1] =  c * scale;
	m[3 * 4 + 0] = tx + cx - (c * scale * cx - s * scale * cy);
	m[3 * 4 + 1] = ty + cy - (s * scale * cx + c * scale * cy);

	std::memcpy(m_world, m, sizeof(m_world));
	if (s_locWorld >= 0)
		glUniformMatrix4fv(s_locWorld, 1, GL_FALSE, m_world);
}

oapi::FMATRIX4 OGLSketchpad::GetWorldTransform() const
{
	oapi::FMATRIX4 out;
	std::memcpy(&out, m_world, sizeof(m_world));
	return out;
}

void OGLSketchpad::PushWorldTransform()
{
	std::array<float, 16> snap;
	std::memcpy(snap.data(), m_world, sizeof(m_world));
	m_worldStack.push_back(snap);
}

void OGLSketchpad::PopWorldTransform()
{
	if (m_worldStack.empty()) return;
	std::memcpy(m_world, m_worldStack.back().data(), sizeof(m_world));
	m_worldStack.pop_back();
	if (s_locWorld >= 0)
		glUniformMatrix4fv(s_locWorld, 1, GL_FALSE, m_world);
}

// --- View / projection ----------------------------------------------------

void OGLSketchpad::SetViewMatrix(const oapi::FMATRIX4 *pV)
{
	if (pV) { std::memcpy(m_view, pV, sizeof(m_view)); m_viewShadow = *pV; }
	else    { MakeIdentity(m_view); float *m = (float*)&m_viewShadow;
	          for (int i = 0; i < 16; i++) m[i] = (i % 5 == 0) ? 1.0f : 0.0f; }
	Mul4x4(m_view, m_proj, m_viewProj);
	std::memcpy(&m_viewProjShadow, m_viewProj, sizeof(m_viewProj));
	if (s_locViewProj >= 0)
		glUniformMatrix4fv(s_locViewProj, 1, GL_FALSE, m_viewProj);
}

void OGLSketchpad::SetProjectionMatrix(const oapi::FMATRIX4 *pP)
{
	if (pP) { std::memcpy(m_proj, pP, sizeof(m_proj)); m_projShadow = *pP; }
	else    { MakeIdentity(m_proj); float *m = (float*)&m_projShadow;
	          for (int i = 0; i < 16; i++) m[i] = (i % 5 == 0) ? 1.0f : 0.0f; }
	Mul4x4(m_view, m_proj, m_viewProj);
	std::memcpy(&m_viewProjShadow, m_viewProj, sizeof(m_viewProj));
	if (s_locViewProj >= 0)
		glUniformMatrix4fv(s_locViewProj, 1, GL_FALSE, m_viewProj);
}

const oapi::FMATRIX4 *OGLSketchpad::ViewMatrix() const { return &m_viewShadow; }
const oapi::FMATRIX4 *OGLSketchpad::ProjectionMatrix() const { return &m_projShadow; }
const oapi::FMATRIX4 *OGLSketchpad::GetViewProjectionMatrix() const { return &m_viewProjShadow; }

void OGLSketchpad::SetViewMode(SkpView mode)
{
	m_viewMode = mode;
	if (s_locViewMode >= 0)
		glUniform1i(s_locViewMode, m_viewMode);
}

// --- Clipping -------------------------------------------------------------

void OGLSketchpad::ClipRect(const LPRECT pClip)
{
	if (pClip) {
		// glScissor uses bottom-left origin; Sketchpad uses top-left. Flip
		// y against the target surface height.
		int x = pClip->left;
		int y = (int)m_viewH - pClip->bottom;
		int w = pClip->right  - pClip->left;
		int h = pClip->bottom - pClip->top;
		glEnable(GL_SCISSOR_TEST);
		glScissor(x, y, std::max(0, w), std::max(0, h));
	} else {
		glDisable(GL_SCISSOR_TEST);
	}
}

void OGLSketchpad::Clipper(int idx, const VECTOR3 *pPos,
                           double cos_angle, double dist)
{
	if (idx < 0 || idx > 1) return;
	if (pPos) {
		const double l = std::sqrt(pPos->x * pPos->x + pPos->y * pPos->y + pPos->z * pPos->z);
		const double inv = l > 1e-9 ? 1.0 / l : 0.0;
		m_clipperDir[idx][0] = (float)(pPos->x * inv);
		m_clipperDir[idx][1] = (float)(pPos->y * inv);
		m_clipperDir[idx][2] = (float)(pPos->z * inv);
		m_clipperDir[idx][3] = 1.0f;
		m_clipperCosDist[idx][0] = (float)cos_angle;
		m_clipperCosDist[idx][1] = (float)dist;
	} else {
		m_clipperDir[idx][3] = 0.0f;  // disabled
	}
	// Re-upload the whole pair so we don't have to track per-slot array
	// uniform offsets.
	float dir[8], cd[4];
	for (int i = 0; i < 2; i++) {
		dir[i*4 + 0] = m_clipperDir[i][0];
		dir[i*4 + 1] = m_clipperDir[i][1];
		dir[i*4 + 2] = m_clipperDir[i][2];
		dir[i*4 + 3] = m_clipperDir[i][3];
		cd[i*2 + 0]  = m_clipperCosDist[i][0];
		cd[i*2 + 1]  = m_clipperCosDist[i][1];
	}
	if (s_locClipperDir     >= 0) glUniform4fv(s_locClipperDir,     2, dir);
	if (s_locClipperCosDist >= 0) glUniform2fv(s_locClipperCosDist, 2, cd);
}

void OGLSketchpad::SetClipDistance(float _near, float _far)
{
	m_clipNear = _near;
	m_clipFar  = _far;
	// glDepthRangef in Core Profile can't change near/far clip planes on
	// its own; those are baked into the projection matrix. We store the
	// values so future SetProjectionMatrix calls (or USER-mode defaults)
	// can read them; nothing in the shipping MFDs/plugins calls this
	// outside of USER mode with a custom projection, where the caller
	// builds the matrix themselves.
}

// --- Metrics --------------------------------------------------------------

void OGLSketchpad::GetRenderSurfaceSize(LPSIZE size)
{
	if (!size) return;
	size->cx = (LONG)m_viewW;
	size->cy = (LONG)m_viewH;
}

// --- World-space primitives ----------------------------------------------

int OGLSketchpad::DrawMeshGroup(const MESHHANDLE hMesh, DWORD grp,
                                MeshFlags flags, const SURFHANDLE hTex)
{
	if (!hMesh) return -1;
	MESHGROUPEX *g = oapiMeshGroupEx(hMesh, grp);
	if (!g || !g->Vtx || !g->Idx || g->nIdx < 3) return -1;

	// Build an interleaved [x,y,u,v] stream. Meshes intended for the
	// Sketchpad path typically place data in the xy plane; we drop z so
	// the existing 2D VBO layout fits.
	std::vector<float> verts(g->nIdx * 4);
	for (DWORD i = 0; i < g->nIdx; i++) {
		const NTVERTEX &v = g->Vtx[g->Idx[i]];
		verts[i * 4 + 0] = v.x;
		verts[i * 4 + 1] = v.y;
		verts[i * 4 + 2] = v.tu;
		verts[i * 4 + 3] = v.tv;
	}

	glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
	glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float),
	             verts.data(), GL_STREAM_DRAW);

	if (hTex) {
		OGLSurface *ts = (OGLSurface*)hTex;
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, ts->GetTexture());
		glUniform1i(s_locTexture, 0);
		const float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		glUniform4fv(s_locColor, 1, white);
		glUniform1i(s_locMode, MODE_TEXTURE);
	} else {
		float c[4]; PackColor(m_brush ? m_brush->col : 0xFFFFFF, c);
		glUniform4fv(s_locColor, 1, c);
		glUniform1i(s_locMode, MODE_SOLID);
	}

	(void)flags;  // SMOOTH_SHADE / CULL_NONE don't affect 2D Sketchpad output
	glDrawArrays(GL_TRIANGLES, 0, (GLsizei)g->nIdx);
	return (int)g->nIdx;
}

// HPOLY is an opaque Orbiter handle for pre-baked polygon geometry.
// clbkCreatePoly (GraphicsAPI.h) returns client-specific concrete types;
// OGLClient does not currently implement that factory, so no HPOLY that
// reaches here would have been produced by us. Accept the handle and
// draw nothing — calling code with a null-handle fallback will see a
// no-op rather than an assert, which matches the "polygon not baked"
// behaviour D3D9 exhibits when clbkCreatePoly returns NULL.
void OGLSketchpad::DrawPoly(const HPOLY /*hPoly*/, DWORD /*flags*/) {}

void OGLSketchpad::SetWorldBillboard(const oapi::FVECTOR3 &wpos, float scl,
                                     bool /*bFixed*/,
                                     const oapi::FVECTOR3 * /*index*/)
{
	// Build a billboard world matrix that translates to `wpos` and
	// applies a uniform scale. The caller is responsible for installing
	// an appropriate view + projection beforehand (USER mode); we leave
	// orientation axis-aligned — the `index` override is an advanced
	// customisation D3D9Pad uses for fixed-axis billboards and is not
	// exercised by any shipping caller.
	float m[16];
	MakeIdentity(m);
	m[0 * 4 + 0] = scl;
	m[1 * 4 + 1] = scl;
	m[2 * 4 + 2] = scl;
	m[3 * 4 + 0] = wpos.x;
	m[3 * 4 + 1] = wpos.y;
	m[3 * 4 + 2] = wpos.z;
	std::memcpy(m_world, m, sizeof(m_world));
	if (s_locWorld >= 0)
		glUniformMatrix4fv(s_locWorld, 1, GL_FALSE, m_world);
}

// --- Misc primitives -------------------------------------------------------

void OGLSketchpad::Lines(const oapi::FVECTOR2 *pt1, int nlines)
{
	if (!pt1 || nlines <= 0 || !m_pen || m_pen->style == 0) return;
	const int nVerts = nlines * 2;
	std::vector<float> xy(nVerts * 2);
	for (int i = 0; i < nVerts; i++) {
		xy[i * 2 + 0] = pt1[i].x + (float)m_ox;
		xy[i * 2 + 1] = pt1[i].y + (float)m_oy;
	}
	DrawSolidLines(xy.data(), nVerts, m_pen->col,
	               (int)std::max(1.0f, m_pen->width * m_lineScale));
}

bool OGLSketchpad::TextW(int x, int y, const LPWSTR str, int len)
{
	if (!str) return false;
	// Convert UTF-16/UCS-2 (LPWSTR on the oapi side is uint16_t*) to UTF-8
	// and fall through to Text(). This keeps glyph lookup in one place
	// while still honouring locale-specific wide strings from Lua scripts.
	const uint16_t *w = (const uint16_t*)str;
	int n = 0;
	if (len > 0) n = len;
	else while (w[n]) n++;

	std::string utf8;
	utf8.reserve(n * 3);
	for (int i = 0; i < n; i++) {
		uint32_t cp = w[i];
		if (cp < 0x80) {
			utf8.push_back((char)cp);
		} else if (cp < 0x800) {
			utf8.push_back((char)(0xC0 | (cp >> 6)));
			utf8.push_back((char)(0x80 | (cp & 0x3F)));
		} else {
			utf8.push_back((char)(0xE0 | (cp >> 12)));
			utf8.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
			utf8.push_back((char)(0x80 | (cp & 0x3F)));
		}
	}
	return Text(x, y, utf8.c_str(), (int)utf8.size());
}

void OGLSketchpad::TextEx(float x, float y, const char *str,
                          float scale, float angle)
{
	if (!str || !*str) return;
	// Scale and rotation operate around the text anchor (x, y). Per-glyph
	// quads are rotated individually so letter spacing follows the angle.
	ImFont *imf = m_font ? m_font->imFont : ImGui::GetFont();
	float sz = (m_font ? m_font->fontSize : 14.0f) * scale;
	if (!imf) return;
	ImFontBaked *baked = imf->GetFontBaked(sz);
	ImFontAtlas *atlas = ImGui::GetIO().Fonts;
	GLuint atlasTex = atlas ? (GLuint)(std::uintptr_t)atlas->TexRef.GetTexID() : 0;
	if (!baked || !atlasTex) return;

	const float c = std::cos(angle), s = std::sin(angle);
	const float ax = x + (float)m_ox, ay = y + (float)m_oy;

	float pen = 0.0f;  // distance along the (rotated) baseline
	std::vector<float> quads;
	const char *end = str + std::strlen(str);
	for (const char *p = str; p < end; ) {
		unsigned int cp = 0;
		int bytes = ImTextCharFromUtf8(&cp, p, end);
		if (bytes <= 0) break;
		p += bytes;
		ImFontGlyph *g = baked->FindGlyph((ImWchar)cp);
		if (!g) continue;

		// See Text() above for why cached UVs are untrusted. (#123)
		ImFontAtlasRect rect;
		float u0 = g->U0, u1 = g->U1, v0 = g->V0, v1 = g->V1;
		if (g->PackId >= 0 && atlas->GetCustomRect(g->PackId, &rect)) {
			u0 = rect.uv0.x; v0 = rect.uv0.y;
			u1 = rect.uv1.x; v1 = rect.uv1.y;
		}

		// Local-space (unrotated) corners.
		const float lx0 = pen + g->X0, lx1 = pen + g->X1;
		const float ly0 = g->Y0,       ly1 = g->Y1;
		auto rot = [&](float lx, float ly, float &ox, float &oy) {
			ox = ax + c * lx - s * ly;
			oy = ay + s * lx + c * ly;
		};
		float rx0, ry0, rx1, ry1, rx2, ry2, rx3, ry3;
		rot(lx0, ly0, rx0, ry0);
		rot(lx1, ly0, rx1, ry1);
		rot(lx1, ly1, rx2, ry2);
		rot(lx0, ly1, rx3, ry3);

		quads.insert(quads.end(), {
			rx0, ry0, u0, v0,
			rx1, ry1, u1, v0,
			rx3, ry3, u0, v1,
			rx1, ry1, u1, v0,
			rx2, ry2, u1, v1,
			rx3, ry3, u0, v1,
		});
		pen += g->AdvanceX;
	}
	if (!quads.empty())
		DrawTexturedQuads(quads.data(), (int)(quads.size() / 4),
		                  atlasTex, m_textCol, MODE_TEXT);
}

} // namespace ogl

#endif // !_WIN32
