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
}

void OGLSketchpad::ReleaseShared()
{
	if (!s_sharedInitialized) return;
	if (s_vao) { glDeleteVertexArrays(1, &s_vao); s_vao = 0; }
	if (s_vbo) { glDeleteBuffers(1, &s_vbo); s_vbo = 0; }
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
	glUniform1i(s_locTexture, 0);
	glUniform4fv(s_locColor, 1, c);
	glUniform1i(s_locMode, mode);
	glDrawArrays(GL_TRIANGLES, 0, nVerts);
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

		float gx0 = pen + glyph->X0;
		float gy0 = py  + glyph->Y0;
		float gx1 = pen + glyph->X1;
		float gy1 = py  + glyph->Y1;

		quads.insert(quads.end(), {
			gx0, gy0, glyph->U0, glyph->V0,
			gx1, gy0, glyph->U1, glyph->V0,
			gx0, gy1, glyph->U0, glyph->V1,
			gx1, gy0, glyph->U1, glyph->V0,
			gx1, gy1, glyph->U1, glyph->V1,
			gx0, gy1, glyph->U0, glyph->V1,
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
	// Low 4 bits pick the colour-combining rule, next nibble picks the
	// texture sampling filter. The filter selection is handed to the
	// OGLSurface sampler we bind in blits — for the immediate draw call
	// we only need to set glBlendFunc for the colour combiner.
	const int combiner = state & 0x0F;
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
	int w = src ? (src->right - src->left) : (int)srcS->GetWidth();
	int h = src ? (src->bottom - src->top) : (int)srcS->GetHeight();
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
	int rw = src ? (src->right - src->left) : (int)srcS->GetWidth();
	int rh = src ? (src->bottom - src->top) : (int)srcS->GetHeight();
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
			rx0, ry0, g->U0, g->V0,
			rx1, ry1, g->U1, g->V0,
			rx3, ry3, g->U0, g->V1,
			rx1, ry1, g->U1, g->V0,
			rx2, ry2, g->U1, g->V1,
			rx3, ry3, g->U0, g->V1,
		});
		pen += g->AdvanceX;
	}
	if (!quads.empty())
		DrawTexturedQuads(quads.data(), (int)(quads.size() / 4),
		                  atlasTex, m_textCol, MODE_TEXT);
}

} // namespace ogl

#endif // !_WIN32
