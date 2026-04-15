// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// OpenGL Graphics Client implementation
// Delegates 3D scene rendering to OGLScene; handles texture/surface/2D drawing

#ifndef _WIN32

#include "OGLClient.h"
#include "OGLTexture.h"
#include "OGLSurface.h"
#include "OGLShaderMgr.h"
#include "OGLScene.h"
#include "OrbiterAPI.h"
#include "VesselAPI.h"
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

// ImGui SDL2+OpenGL3 backends
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"

namespace ogl {

// ============================================================================
// Helpers
// ============================================================================

static bool FileExists(const char *path) {
	struct stat st;
	return stat(path, &st) == 0;
}

// Helper: convert Orbiter 0xBBGGRR colour to ImGui 0xAABBGGRR (alpha = 0xFF)
static ImU32 OrbiterColToImU32(DWORD col) {
	return IM_COL32(col & 0xFF, (col >> 8) & 0xFF, (col >> 16) & 0xFF, 0xFF);
}

// ============================================================================
// OGLClient
// ============================================================================

OGLClient::OGLClient(HINSTANCE hInstance)
	: GraphicsClient(hInstance),
	  m_sdlWindow(nullptr), m_sdlContext(nullptr),
	  m_viewW(1280), m_viewH(800), m_fullscreen(false),
	  m_imguiInitialized(false),
	  m_shaderMgr(nullptr), m_scene(nullptr)
{
	fprintf(stderr, "[OGLClient] Created\n");
}

OGLClient::~OGLClient()
{
	fprintf(stderr, "[OGLClient] Destroyed\n");
}

// Pure virtuals

bool OGLClient::clbkFullscreenMode() const { return m_fullscreen; }

void OGLClient::clbkGetViewportSize(DWORD *w, DWORD *h) const {
	*w = m_viewW; *h = m_viewH;
}

bool OGLClient::clbkGetRenderParam(DWORD prm, DWORD *value) const {
	switch (prm) {
	case 0x100: *value = 32; return true;
	case 0x101: *value = 24; return true;
	case 0x102: *value = 8;  return true;
	case 0x103: *value = 8;  return true;
	default: *value = 0; return false;
	}
}

// ============================================================================
// ImGui
// ============================================================================

void OGLClient::clbkImGuiInit() {
	if (m_imguiInitialized) return;
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;
	ImGui::StyleColorsDark();
	ImGui_ImplSDL2_InitForOpenGL(m_sdlWindow, m_sdlContext);
	ImGui_ImplOpenGL3_Init("#version 410");
	m_imguiInitialized = true;
}

void OGLClient::clbkImGuiShutdown() {
	if (!m_imguiInitialized) return;
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
	m_imguiInitialized = false;
}

void OGLClient::clbkImGuiNewFrame() {
	if (!m_imguiInitialized) return;
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame();
}

void OGLClient::clbkImGuiRenderDrawData() {
	if (!m_imguiInitialized) return;
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

uint64_t OGLClient::clbkImGuiSurfaceTexture(SURFHANDLE surf) {
	if (!surf) return 0;
	OGLSurface *s = (OGLSurface*)surf;
	return (uint64_t)s->GetTexture();
}

// ============================================================================
// Render scene — delegates to OGLScene
// ============================================================================

void OGLClient::clbkRenderScene()
{
	if (m_sdlWindow) {
		int w, h;
		SDL_GL_GetDrawableSize(m_sdlWindow, &w, &h);
		m_viewW = w;
		m_viewH = h;
	}

	if (m_scene)
		m_scene->RenderScene(m_viewW, m_viewH);

	Render2DOverlay();
}

// ============================================================================
// Session lifecycle
// ============================================================================

HWND OGLClient::clbkCreateRenderWindow()
{
	fprintf(stderr, "[OGLClient] clbkCreateRenderWindow\n");

	if (m_sdlWindow) {
		int w, h;
		SDL_GL_GetDrawableSize(m_sdlWindow, &w, &h);
		m_viewW = w;
		m_viewH = h;
	}

	// Determine texture search path
	char cwd[1024];
	if (getcwd(cwd, sizeof(cwd))) {
		m_texturePath = std::string(cwd) + "/Textures/";
		struct stat st;
		if (stat(m_texturePath.c_str(), &st) != 0) {
			m_texturePath = std::string(cwd) + "/../Textures/";
			if (stat(m_texturePath.c_str(), &st) != 0)
				m_texturePath = "Textures/";
		}
	} else {
		m_texturePath = "Textures/";
	}
	fprintf(stderr, "[OGLClient] Texture path: %s\n", m_texturePath.c_str());

	// Initialize shader manager
	m_shaderMgr = new ShaderMgr();
	// Shader path: try "shaders/" relative to cwd, fallback to source tree
	{
		std::string shaderPath;
		if (getcwd(cwd, sizeof(cwd))) {
			shaderPath = std::string(cwd) + "/shaders/";
			struct stat st;
			if (stat(shaderPath.c_str(), &st) != 0)
				shaderPath = "shaders/";
		} else {
			shaderPath = "shaders/";
		}
		m_shaderMgr->SetShaderPath(shaderPath);
		fprintf(stderr, "[OGLClient] Shader path: %s\n", shaderPath.c_str());
	}

	// Initialize scene
	m_scene = new OGLScene(m_shaderMgr);
	m_scene->Init(m_texturePath);

	fprintf(stderr, "[OGLClient] Scene initialized\n");
	return (HWND)m_sdlWindow;
}

void OGLClient::clbkDestroyRenderWindow(bool fastclose)
{
	fprintf(stderr, "[OGLClient] clbkDestroyRenderWindow\n");

	delete m_scene; m_scene = nullptr;
	delete m_shaderMgr; m_shaderMgr = nullptr;
}

bool OGLClient::clbkDisplayFrame()
{
	if (m_sdlWindow) { SDL_GL_SwapWindow(m_sdlWindow); return true; }
	return false;
}

void OGLClient::clbkUpdate(bool) {}
void OGLClient::clbkPostCreation() { fprintf(stderr, "[OGLClient] Scenario loaded\n"); }
void OGLClient::clbkCloseSession(bool) { fprintf(stderr, "[OGLClient] Session closed\n"); }

// ============================================================================
// Texture / Surface operations
// ============================================================================

// Helper: load a texture file and wrap it in an OGLSurface
static OGLSurface *LoadTextureAsSurface(const char *path)
{
	OGLTexture *tex = OGLTexture::LoadTexture(path);
	if (!tex) return nullptr;
	OGLSurface *surf = new OGLSurface();
	surf->WrapTexture(tex->texId, tex->width, tex->height);
	// Prevent OGLTexture destructor from deleting the GL texture
	// since OGLSurface now owns it
	tex->texId = 0;
	delete tex;
	return surf;
}

SURFHANDLE OGLClient::clbkLoadTexture(const char *fname, DWORD flags)
{
	if (!fname || !fname[0]) return nullptr;

	std::string normalizedName = fname;
	for (auto &c : normalizedName) if (c == '\\') c = '/';

	std::string tryPath = m_texturePath + normalizedName;
	if (FileExists(tryPath.c_str())) {
		OGLSurface *s = LoadTextureAsSurface(tryPath.c_str());
		if (s) return (SURFHANDLE)s;
	}

	if (FileExists(normalizedName.c_str())) {
		OGLSurface *s = LoadTextureAsSurface(normalizedName.c_str());
		if (s) return (SURFHANDLE)s;
	}

	const char *ext = strrchr(normalizedName.c_str(), '.');
	if (!ext) {
		const char *tryExts[] = { ".dds", ".bmp", ".tex", ".png", nullptr };
		for (int i = 0; tryExts[i]; i++) {
			std::string tryP = m_texturePath + normalizedName + tryExts[i];
			if (FileExists(tryP.c_str())) {
				OGLSurface *s = LoadTextureAsSurface(tryP.c_str());
				if (s) return (SURFHANDLE)s;
			}
		}
	}

	fprintf(stderr, "[OGLClient] clbkLoadTexture: not found '%s'\n", fname);
	return nullptr;
}

void OGLClient::clbkReleaseTexture(SURFHANDLE hTex)
{
	if (!hTex) return;
	OGLSurface *surf = (OGLSurface*)hTex;
	surf->Release();
}

bool OGLClient::clbkReleaseSurface(SURFHANDLE surf)
{
	if (!surf) return false;
	OGLSurface *s = (OGLSurface*)surf;
	s->Release();
	return true;
}

SURFHANDLE OGLClient::clbkCreateSurfaceEx(DWORD w, DWORD h, DWORD attrib)
{
	OGLSurface *surf = new OGLSurface();
	if (!surf->Create(w, h, attrib)) {
		delete surf;
		return nullptr;
	}
	return (SURFHANDLE)surf;
}

bool OGLClient::clbkGetSurfaceSize(SURFHANDLE surf, DWORD *w, DWORD *h)
{
	if (!surf) { *w = *h = 0; return false; }
	OGLSurface *s = (OGLSurface*)surf;
	*w = s->GetWidth();
	*h = s->GetHeight();
	return true;
}

void OGLClient::clbkIncrSurfaceRef(SURFHANDLE surf)
{
	if (surf)
		((OGLSurface*)surf)->AddRef();
}

bool OGLClient::clbkSetSurfaceColourKey(SURFHANDLE surf, DWORD ckey)
{
	if (!surf) return false;
	((OGLSurface*)surf)->SetColorKey(ckey);
	return true;
}

DWORD OGLClient::clbkGetDeviceColour(BYTE r, BYTE g, BYTE b)
{
	return (DWORD)r | ((DWORD)g << 8) | ((DWORD)b << 16);
}

// ============================================================================
// OGL Font / Pen / Brush / Sketchpad — ImGui-backed 2D drawing
// ============================================================================

class OGLFont : public oapi::Font {
public:
	ImFont *imFont;
	float  fontSize;
	OGLFont(int height, bool prop, const char *face, FontStyle style, int orientation)
		: oapi::Font(height, prop, face, style, orientation),
		  imFont(nullptr), fontSize((float)(height < 0 ? -height : height))
	{
		ImGuiIO &io = ImGui::GetIO();
		if (!io.Fonts || io.Fonts->Fonts.Size == 0) return;
		if (!prop || (face && (strcmp(face, "Fixed") == 0 || strcmp(face, "Courier") == 0 ||
		              strcmp(face, "Courier New") == 0))) {
			if (io.Fonts->Fonts.Size > 2) imFont = io.Fonts->Fonts[2];
			else imFont = io.Fonts->Fonts[0];
		} else {
			imFont = io.Fonts->Fonts[0];
		}
	}
};

class OGLPen : public oapi::Pen {
public:
	int    style;
	int    width;
	ImU32  color;
	OGLPen(int s, int w, DWORD col) : oapi::Pen(s, w, col), style(s), width(w), color(OrbiterColToImU32(col)) {}
};

class OGLBrush : public oapi::Brush {
public:
	ImU32 color;
	OGLBrush(DWORD col) : oapi::Brush(col), color(OrbiterColToImU32(col)) {}
};

class OGLSketchpad : public oapi::Sketchpad {
public:
	OGLSketchpad(SURFHANDLE surf, DWORD w, DWORD h)
		: oapi::Sketchpad(surf), m_dl(nullptr), m_font(nullptr), m_pen(nullptr), m_brush(nullptr),
		  m_textCol(IM_COL32(255,255,255,255)), m_bgCol(IM_COL32(0,0,0,255)),
		  m_bkMode(BK_TRANSPARENT), m_tah(LEFT), m_tav(TOP), m_cx(0), m_cy(0),
		  m_ox(0), m_oy(0), m_viewW(w), m_viewH(h)
	{
		m_dl = ImGui::GetForegroundDrawList();
	}

	oapi::Font *SetFont(oapi::Font *font) override {
		oapi::Font *prev = m_font; m_font = font; return prev;
	}
	oapi::Pen *SetPen(oapi::Pen *pen) override {
		oapi::Pen *prev = m_pen; m_pen = pen; return prev;
	}
	oapi::Brush *SetBrush(oapi::Brush *brush) override {
		oapi::Brush *prev = m_brush; m_brush = brush; return prev;
	}

	void SetTextAlign(TAlign_horizontal tah = LEFT, TAlign_vertical tav = TOP) override {
		m_tah = tah; m_tav = tav;
	}
	DWORD SetTextColor(DWORD col) override {
		DWORD prev = ImGuiColToOrbiter(m_textCol);
		m_textCol = OrbiterColToImU32(col);
		return prev;
	}
	DWORD SetBackgroundColor(DWORD col) override {
		DWORD prev = ImGuiColToOrbiter(m_bgCol);
		m_bgCol = OrbiterColToImU32(col);
		return prev;
	}
	void SetBackgroundMode(BkgMode mode) override { m_bkMode = mode; }
	void SetOrigin(int x, int y) override { m_ox = x; m_oy = y; }
	void GetOrigin(int *x, int *y) const override { *x = m_ox; *y = m_oy; }

	DWORD GetCharSize() override {
		OGLFont *f = static_cast<OGLFont*>(m_font);
		float sz = f ? f->fontSize : 14.0f;
		return MAKELONG((int)(sz * 0.6f), (int)sz);
	}

	DWORD GetTextWidth(const char *str, int len = 0) override {
		OGLFont *f = static_cast<OGLFont*>(m_font);
		ImFont *imf = f ? f->imFont : ImGui::GetFont();
		float sz = f ? f->fontSize : 14.0f;
		if (!str) return 0;
		std::string s = (len > 0) ? std::string(str, len) : std::string(str);
		ImVec2 tsz = imf->CalcTextSizeA(sz, FLT_MAX, 0, s.c_str());
		return (DWORD)tsz.x;
	}

	bool Text(int x, int y, const char *str, int len) override {
		if (!m_dl || !str) return false;
		OGLFont *f = static_cast<OGLFont*>(m_font);
		ImFont *imf = f ? f->imFont : ImGui::GetFont();
		float sz = f ? f->fontSize : 14.0f;
		std::string s = (len > 0) ? std::string(str, len) : std::string(str);
		ImVec2 tsz = imf->CalcTextSizeA(sz, FLT_MAX, 0, s.c_str());
		float dx = (float)(x + m_ox), dy = (float)(y + m_oy);
		if (m_tah == CENTER) dx -= tsz.x * 0.5f;
		else if (m_tah == RIGHT) dx -= tsz.x;
		if (m_tav == BASELINE) dy -= sz * 0.8f;
		else if (m_tav == BOTTOM) dy -= tsz.y;
		if (m_bkMode == BK_OPAQUE)
			m_dl->AddRectFilled(ImVec2(dx, dy), ImVec2(dx + tsz.x, dy + tsz.y), m_bgCol);
		m_dl->AddText(imf, sz, ImVec2(dx, dy), m_textCol, s.c_str());
		return true;
	}

	void MoveTo(int x, int y) override { m_cx = x + m_ox; m_cy = y + m_oy; }

	void LineTo(int x, int y) override {
		int nx = x + m_ox, ny = y + m_oy;
		OGLPen *p = static_cast<OGLPen*>(m_pen);
		if (m_dl && p && p->style != 0)
			m_dl->AddLine(ImVec2((float)m_cx, (float)m_cy), ImVec2((float)nx, (float)ny),
			              p->color, (float)std::max(1, p->width));
		m_cx = nx; m_cy = ny;
	}

	void Line(int x0, int y0, int x1, int y1) override {
		OGLPen *p = static_cast<OGLPen*>(m_pen);
		if (m_dl && p && p->style != 0)
			m_dl->AddLine(ImVec2((float)(x0+m_ox), (float)(y0+m_oy)),
			              ImVec2((float)(x1+m_ox), (float)(y1+m_oy)),
			              p->color, (float)std::max(1, p->width));
	}

	void Rectangle(int x0, int y0, int x1, int y1) override {
		if (!m_dl) return;
		ImVec2 a((float)(x0+m_ox), (float)(y0+m_oy)), b((float)(x1+m_ox), (float)(y1+m_oy));
		OGLBrush *br = static_cast<OGLBrush*>(m_brush);
		if (br) m_dl->AddRectFilled(a, b, br->color);
		OGLPen *p = static_cast<OGLPen*>(m_pen);
		if (p && p->style != 0) m_dl->AddRect(a, b, p->color, 0, 0, (float)std::max(1, p->width));
	}

	void Ellipse(int x0, int y0, int x1, int y1) override {
		if (!m_dl) return;
		float cx = (x0 + x1) * 0.5f + m_ox, cy = (y0 + y1) * 0.5f + m_oy;
		float rx = (x1 - x0) * 0.5f, ry = (y1 - y0) * 0.5f;
		float r = (rx + ry) * 0.5f;
		OGLBrush *br = static_cast<OGLBrush*>(m_brush);
		if (br) m_dl->AddCircleFilled(ImVec2(cx, cy), r, br->color);
		OGLPen *p = static_cast<OGLPen*>(m_pen);
		if (p && p->style != 0) m_dl->AddCircle(ImVec2(cx, cy), r, p->color, 0, (float)std::max(1, p->width));
	}

	void Polygon(const oapi::IVECTOR2 *pt, int npt) override {
		if (!m_dl || npt < 3) return;
		std::vector<ImVec2> pts(npt);
		for (int i = 0; i < npt; i++) pts[i] = ImVec2((float)(pt[i].x+m_ox), (float)(pt[i].y+m_oy));
		OGLBrush *br = static_cast<OGLBrush*>(m_brush);
		if (br) m_dl->AddConvexPolyFilled(pts.data(), npt, br->color);
		OGLPen *p = static_cast<OGLPen*>(m_pen);
		if (p && p->style != 0) m_dl->AddPolyline(pts.data(), npt, p->color, ImDrawFlags_Closed, (float)std::max(1, p->width));
	}

	void Polyline(const oapi::IVECTOR2 *pt, int npt) override {
		if (!m_dl || npt < 2) return;
		OGLPen *p = static_cast<OGLPen*>(m_pen);
		if (!p || p->style == 0) return;
		std::vector<ImVec2> pts(npt);
		for (int i = 0; i < npt; i++) pts[i] = ImVec2((float)(pt[i].x+m_ox), (float)(pt[i].y+m_oy));
		m_dl->AddPolyline(pts.data(), npt, p->color, 0, (float)std::max(1, p->width));
	}

	void Pixel(int x, int y, DWORD col) override {
		if (m_dl) m_dl->AddRectFilled(ImVec2((float)(x+m_ox), (float)(y+m_oy)),
		                               ImVec2((float)(x+m_ox+1), (float)(y+m_oy+1)),
		                               OrbiterColToImU32(col));
	}

private:
	static DWORD ImGuiColToOrbiter(ImU32 c) {
		return (c & 0xFF) | ((c >> 8) & 0xFF) << 8 | ((c >> 16) & 0xFF) << 16;
	}
	ImDrawList *m_dl;
	oapi::Font *m_font;
	oapi::Pen  *m_pen;
	oapi::Brush *m_brush;
	ImU32 m_textCol, m_bgCol;
	BkgMode m_bkMode;
	TAlign_horizontal m_tah;
	TAlign_vertical m_tav;
	int m_cx, m_cy;
	int m_ox, m_oy;
	DWORD m_viewW, m_viewH;
};

// --- clbk implementations ---

oapi::Font *OGLClient::clbkCreateFont(int height, bool prop, const char *face,
	FontStyle style, int orientation) const
{
	return new OGLFont(height, prop, face, style, orientation);
}

void OGLClient::clbkReleaseFont(oapi::Font *font) const { delete font; }

oapi::Pen *OGLClient::clbkCreatePen(int style, int width, DWORD col) const {
	return new OGLPen(style, width, col);
}
void OGLClient::clbkReleasePen(oapi::Pen *pen) const { delete pen; }

oapi::Brush *OGLClient::clbkCreateBrush(DWORD col) const {
	return new OGLBrush(col);
}
void OGLClient::clbkReleaseBrush(oapi::Brush *brush) const { delete brush; }

oapi::Sketchpad *OGLClient::clbkGetSketchpad(SURFHANDLE surf) {
	if (!m_imguiInitialized) return nullptr;
	if (!ImGui::GetCurrentContext()) return nullptr;
	if (!ImGui::GetIO().Fonts || !ImGui::GetIO().Fonts->IsBuilt()) return nullptr;
	return new OGLSketchpad(surf, m_viewW, m_viewH);
}

void OGLClient::clbkReleaseSketchpad(oapi::Sketchpad *sp) { delete sp; }

void OGLClient::SetSDLWindow(SDL_Window *w, SDL_GLContext c) { m_sdlWindow = w; m_sdlContext = c; }

} // namespace ogl

#endif // !_WIN32
