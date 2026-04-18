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
bool   OGLSketchpad::s_sharedInitialized = false;

void OGLSketchpad::InitShared(ShaderMgr *sm)
{
	if (s_sharedInitialized) return;
	s_sharedInitialized = true;

	s_program = sm->LoadProgram("sketchpad", "sketchpad.vert", "sketchpad.frag");
	s_locViewport = sm->GetUniformLoc(s_program, "uViewport");
	s_locColor    = sm->GetUniformLoc(s_program, "uColor");
	s_locMode     = sm->GetUniformLoc(s_program, "uMode");
	s_locTexture  = sm->GetUniformLoc(s_program, "uTexture");

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
	  m_cx(0), m_cy(0), m_ox(0), m_oy(0)
{
	if (m_surf) {
		m_viewW = m_surf->GetWidth();
		m_viewH = m_surf->GetHeight();
	}
	BindState();
}

OGLSketchpad::~OGLSketchpad()
{
	UnbindState();
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

void OGLSketchpad::PackColor(DWORD col, float out[4])
{
	// Orbiter stores 0xBBGGRR. An alpha byte in bits 24-31 is optional
	// (D3D9 flagging); zero alpha is treated as fully opaque — this matches
	// the oapi::ColorCompatibility default every client ships with.
	float r = ((col >> 16) & 0xFF) / 255.0f;
	float g = ((col >>  8) & 0xFF) / 255.0f;
	float b = ( col        & 0xFF) / 255.0f;
	float a = ((col >> 24) & 0xFF) / 255.0f;
	if (a < 1.0f / 255.0f) a = 1.0f;
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

} // namespace ogl

#endif // !_WIN32
