// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// OpenGL Graphics Client implementation
// Delegates 3D scene rendering to OGLScene; handles texture/surface/2D drawing

#ifndef _WIN32

#include "OGLClient.h"
#include "Util.h"  // ResolvePathIgnoreCase
#include "OGLTexture.h"
#include "OGLSurface.h"
#include "OGLShaderMgr.h"
#include "OGLMaterial.h"
#include "OGLMeshRegistry.h"
#include "OGLScene.h"
#include "OGLvVessel.h"
#include "OGLSketchpad.h"
#include "OGLPostProcess.h"
#include "OGLParticle.h"
#include "OGLAnnotation.h"
#include "OGLBeaconArray.h"
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

// gcGUI side-bar host. gcGetGUICore() is exported from OGLWindowMgr.cpp
// and resolved at run time by gcGUIApp::Initialize via dlsym
// (RTLD_DEFAULT). Included before the namespace block so the nested
// `ogl::OGLWindowMgr` is reachable as `ogl::OGLWindowMgr` rather than
// `ogl::ogl::OGLWindowMgr` from inside this TU.
#include "OGLWindowMgr.h"

namespace ogl {

// ============================================================================
// Helpers
// ============================================================================

static bool FileExists(const char *path) {
	struct stat st;
	return stat(path, &st) == 0;
}

// ============================================================================
// OGLClient
// ============================================================================

OGLClient::OGLClient(HINSTANCE hInstance)
	: GraphicsClient(hInstance),
	  m_sdlWindow(nullptr), m_sdlContext(nullptr),
	  m_viewW(1280), m_viewH(800), m_fullscreen(false),
	  m_imguiInitialized(false),
	  m_shaderMgr(nullptr), m_scene(nullptr), m_postProcess(nullptr),
	  m_blitShader(0), m_blitVAO(0), m_blitVBO(0),
	  m_panel2dShader(0), m_bltGroupTgt(nullptr)
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

	// Register the gcGUI smoke-test app if the operator opted in via
	// ORBITER_GCGUI_TEST=1 — verifies the dlsym dispatch end-to-end.
	ogl::MaybeStartGcGuiSmokeApp();
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

void OGLClient::clbkRenderImGuiPlugins() {
	ogl::OGLWindowMgr::Instance().RenderAll();
}

// stb_image_write — vendored under OVP/OGLClient/stb_image_write.h.
// Define implementation in this TU so the writer is linked once.
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

bool OGLClient::clbkSaveScreenshot(const char *path, int w, int h) {
	if (!path || !*path) return false;
	int vw = w, vh = h;
	if (vw <= 0 || vh <= 0) {
		if (m_sdlWindow) {
			SDL_GL_GetDrawableSize(m_sdlWindow, &vw, &vh);
		} else {
			GLint vp[4] = {0};
			glGetIntegerv(GL_VIEWPORT, vp);
			vw = vp[2]; vh = vp[3];
		}
	}
	if (vw <= 0 || vh <= 0) return false;

	std::vector<unsigned char> pixels((size_t)vw * vh * 4);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	// Force the driver to retire every pending draw before we sample the
	// backbuffer. The macOS OpenGL 4.1-via-Metal stack is more lenient
	// with implicit glReadPixels synchronisation than native GL, and a
	// missing flush here historically produced all-black captures even
	// when the interactive window rendered the scene correctly.
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	glFinish();
	// glReadBuffer is not allowed in core profile for default
	// framebuffer reads of GL_BACK on macOS — but glReadPixels
	// against the default framebuffer reads from GL_BACK by default
	// after a draw, so we can skip the explicit glReadBuffer call.
	glReadPixels(0, 0, vw, vh, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

	// Flip vertically: OpenGL origin is bottom-left, PNG / image
	// loaders treat origin as top-left.
	std::vector<unsigned char> flipped((size_t)vw * vh * 4);
	const size_t row = (size_t)vw * 4;
	for (int y = 0; y < vh; ++y) {
		std::memcpy(flipped.data() + (size_t)y * row,
		            pixels.data()  + (size_t)(vh - 1 - y) * row,
		            row);
	}

	int rc = stbi_write_png(path, vw, vh, 4, flipped.data(), (int)row);
	if (!rc) {
		fprintf(stderr, "[OGLClient] screenshot write failed: %s\n", path);
		return false;
	}
	fprintf(stderr, "[OGLClient] screenshot saved: %s (%dx%d)\n", path, vw, vh);
	return true;
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

	// Debug-only hot-reload: polls shader file mtimes every N frames and
	// rebuilds affected programs in place. No-op in release builds.
	if (m_shaderMgr)
		m_shaderMgr->CheckReload();

	// Periodic (≥5 s) one-liner with mesh-cache counters — proves the
	// registry is actually servicing hits rather than re-uploading every
	// mesh each frame.
	MeshRegistry::Instance().LogStatsPeriodic();

	// M11 post-processing: route the 3D scene through the HDR RGBA16F FBO so
	// bloom + tone map + lens flare can run before the frame hits the
	// backbuffer. Opt-out via OGL_NO_POSTFX=1 for low-spec hardware.
	static const bool s_postFxDisabled = []() {
		const char *v = std::getenv("OGL_NO_POSTFX");
		return v && v[0] == '1';
	}();
	const bool useHDR = m_postProcess && !s_postFxDisabled;

	if (useHDR) {
		m_postProcess->Resize(m_viewW, m_viewH);
		m_postProcess->BeginScene();
	}

	if (m_scene)
		m_scene->RenderScene(m_viewW, m_viewH);

	if (useHDR) {
		float sunX = 0.0f, sunY = 0.0f;
		bool  sunVisible = false;
		m_scene->GetSunNDC(sunX, sunY, sunVisible);
		m_postProcess->EndScene(sunX, sunY, sunVisible);
	}

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

	// Wire the vessel renderer up to this client so the VC pass can reach
	// GetVCMFDSurface / GetVCHUDSurface through the virtual interface.
	ogl::OGLvVessel::SetGraphicsClient(this);

	// Initialize blit/panel resources
	InitBlitResources();

	// Initialize particle and beacon systems
	OGLParticleStream::InitShared(m_shaderMgr, m_texturePath);
	OGLBeaconArray::InitShared(m_shaderMgr);

	// Initialize post-processing pipeline
	m_postProcess = new OGLPostProcess(m_shaderMgr);
	m_postProcess->Init(m_viewW, m_viewH);

	// All shader programs are now linked; emit a one-shot summary so the
	// log makes it obvious which programs, files and UBO blocks are live.
	m_shaderMgr->LogStatus();

	// M1 self-test: round-trips an RGBA8 render target and verifies that
	// FBO bind / clear / readback / unbind all work on the current GL
	// context. Gated by OGL_M1_SELFTEST=1 so it stays out of production.
	if (const char *v = std::getenv("OGL_M1_SELFTEST"); v && v[0] == '1')
		RunM1SelfTest();

	fprintf(stderr, "[OGLClient] Scene initialized\n");
	return (HWND)m_sdlWindow;
}

void OGLClient::clbkDestroyRenderWindow(bool fastclose)
{
	fprintf(stderr, "[OGLClient] clbkDestroyRenderWindow\n");

	delete m_postProcess; m_postProcess = nullptr;
	OGLParticleStream::ReleaseShared();
	OGLBeaconArray::ReleaseShared();
	ReleaseBlitResources();
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

	// Each lookup candidate is first probed literally, then via the
	// case-insensitive resolver so references authored with Win32
	// casing (e.g. "Atlantis\\MGAtlantis.dds" → "atlantis/mgatlantis.dds"
	// on disk) resolve on POSIX without a manual rename pass.
	auto tryLoad = [&](const std::string &path) -> OGLSurface* {
		if (FileExists(path.c_str()))
			return LoadTextureAsSurface(path.c_str());
		const std::string resolved = ResolvePathIgnoreCase(path.c_str());
		if (!resolved.empty() && resolved != path)
			return LoadTextureAsSurface(resolved.c_str());
		return nullptr;
	};

	if (OGLSurface *s = tryLoad(m_texturePath + normalizedName)) return (SURFHANDLE)s;
	if (OGLSurface *s = tryLoad(normalizedName))                 return (SURFHANDLE)s;

	const char *ext = strrchr(normalizedName.c_str(), '.');
	if (!ext) {
		const char *tryExts[] = { ".dds", ".bmp", ".tex", ".png", nullptr };
		for (int i = 0; tryExts[i]; i++) {
			if (OGLSurface *s = tryLoad(m_texturePath + normalizedName + tryExts[i]))
				return (SURFHANDLE)s;
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

// Plain 2-arg overload used by oapiCreateSurface(w,h). Legacy Orbiter API —
// callers (VectorMap, DlgMap, MFD panels, various vessel modules) expect a
// render-target texture they can feed into Sketchpad and later show via
// ImGui or blit as a mesh texture. Without this override the base class
// returned NULL and DlgMap stayed empty (#39).
SURFHANDLE OGLClient::clbkCreateSurface(DWORD w, DWORD h, SURFHANDLE /*hTemplate*/)
{
	if (w == 0 || h == 0) return nullptr;
	return clbkCreateSurfaceEx(w, h,
		OAPISURFACE_TEXTURE | OAPISURFACE_RENDERTARGET | OAPISURFACE_SKETCHPAD);
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

bool OGLClient::clbkClearSurfaceColourKey(SURFHANDLE surf)
{
	if (!surf) return false;
	((OGLSurface*)surf)->ClearColorKey();
	return true;
}

DWORD OGLClient::clbkGetDeviceColour(BYTE r, BYTE g, BYTE b)
{
	return (DWORD)r | ((DWORD)g << 8) | ((DWORD)b << 16);
}

// ============================================================================
// Blit system initialization
// ============================================================================

void OGLClient::InitBlitResources()
{
	m_blitShader = m_shaderMgr->LoadProgram("blit", "blit.vert", "blit.frag");
	m_panel2dShader = m_shaderMgr->LoadProgram("panel2d", "panel2d.vert", "panel2d.frag");
	m_bltGroupTgt = nullptr;

	// Create a unit quad (positions and UVs updated per-blit via glBufferSubData)
	float verts[] = {
		// pos x,y  uv u,v
		-1, -1,  0, 1,
		 1, -1,  1, 1,
		-1,  1,  0, 0,
		 1,  1,  1, 0,
	};
	glGenVertexArrays(1, &m_blitVAO);
	glGenBuffers(1, &m_blitVBO);
	glBindVertexArray(m_blitVAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_blitVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
	glEnableVertexAttribArray(1);
	glBindVertexArray(0);
}

void OGLClient::ReleaseBlitResources()
{
	if (m_blitVAO) { glDeleteVertexArrays(1, &m_blitVAO); m_blitVAO = 0; }
	if (m_blitVBO) { glDeleteBuffers(1, &m_blitVBO); m_blitVBO = 0; }
	m_blitShader = 0;
	m_panel2dShader = 0;
}

// Core blit helper: renders a textured quad from src rect to tgt rect.
// When `tgt` is null the quad is drawn into the default framebuffer
// (backbuffer) at its current viewport — this is what scripts and MFD
// plugins rely on when they pass NULL for the target surface.
void OGLClient::BlitQuad(OGLSurface *tgt, DWORD tgtx, DWORD tgty, DWORD tgtw, DWORD tgth,
                          OGLSurface *src, DWORD srcx, DWORD srcy, DWORD srcw, DWORD srch,
                          DWORD flag) const
{
	if (!m_blitShader || !m_blitVAO || !src) return;

	// Compute source UV coords
	float sw = (float)src->GetWidth(), sh = (float)src->GetHeight();
	float u0 = (float)srcx / sw, v0 = (float)srcy / sh;
	float u1 = (float)(srcx + srcw) / sw, v1 = (float)(srcy + srch) / sh;

	// Target extent is either the surface's own size or the backbuffer viewport.
	const float tw = tgt ? (float)tgt->GetWidth()  : (float)m_viewW;
	const float th = tgt ? (float)tgt->GetHeight() : (float)m_viewH;
	if (tw <= 0.0f || th <= 0.0f) return;

	// Compute target NDC coords within the render-target viewport.
	float x0 = (float)tgtx / tw * 2.0f - 1.0f;
	float y0 = 1.0f - (float)(tgty + tgth) / th * 2.0f; // flip y for OpenGL
	float x1 = (float)(tgtx + tgtw) / tw * 2.0f - 1.0f;
	float y1 = 1.0f - (float)tgty / th * 2.0f;

	// UV orientation depends on whether the source has been *rendered into*
	// (FBO-backed render target, e.g., sketchpad MFD surface) or was uploaded
	// from an image file. FBO-rendered content stores its visual top at OGL
	// texture v=1; file-loaded textures store their visual top at v=0. The
	// blit must pair "NDC top of dst" with the source's visual-top v so the
	// rendered output keeps the same orientation as the input. Without this
	// split, chained FBO→FBO→FBO blits (MFD tapes1→surf→tex, #58) picked up
	// an extra Y-flip per hop and the tape/horizon labels came out mirrored.
	const DWORD rtMask = OAPISURFACE_RENDERTARGET | OAPISURFACE_SKETCHPAD |
	                     OAPISURFACE_GDI | OAPISURFACE_RENDER3D;
	const bool srcIsRT = (src->GetAttrib() & rtMask) != 0;
	const float vTop = srcIsRT ? v1 : v0;
	const float vBot = srcIsRT ? v0 : v1;

	float verts[] = {
		x0, y0, u0, vBot,
		x1, y0, u1, vBot,
		x0, y1, u0, vTop,
		x1, y1, u1, vTop,
	};

	// Bind target. For a surface target use BindFBO() (which also handles MSAA
	// + mipmap regen in UnbindFBO); for the backbuffer, bind FBO 0 manually
	// and remember to restore the caller's state when we're done.
	GLint bbFBO = 0;
	GLint bbVp[4] = { 0, 0, 0, 0 };
	if (tgt) {
		tgt->BindFBO();
	} else {
		glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &bbFBO);
		glGetIntegerv(GL_VIEWPORT, bbVp);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, (GLsizei)m_viewW, (GLsizei)m_viewH);
	}

	glUseProgram(m_blitShader);
	glBindVertexArray(m_blitVAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_blitVBO);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, src->GetTexture());
	glUniform1i(m_shaderMgr->GetUniformLoc(m_blitShader, "uTexture"), 0);
	glUniform1f(m_shaderMgr->GetUniformLoc(m_blitShader, "uAlpha"), 1.0f);

	// Color key handling
	bool useCK = (flag & BLT_SRCCOLORKEY) && src->HasColorKey();
	glUniform1i(m_shaderMgr->GetUniformLoc(m_blitShader, "uUseColorKey"), useCK ? 1 : 0);
	if (useCK) {
		DWORD ck = src->GetColorKey();
		float ckr = (ck & 0xFF) / 255.0f;
		float ckg = ((ck >> 8) & 0xFF) / 255.0f;
		float ckb = ((ck >> 16) & 0xFF) / 255.0f;
		glUniform3f(m_shaderMgr->GetUniformLoc(m_blitShader, "uColorKey"), ckr, ckg, ckb);
	}

	// DX7 IDirectDrawSurface::Blt is a raw pixel copy (with optional colorkey
	// discard), not an alpha blend. Leaving GL_BLEND on here made every blt
	// composite src over dst, so MFD surface→texture copies (Mfd.cpp:812,
	// after Fill(0x000000) which clears to alpha=0) retained the previous
	// frame's content at every cleared pixel — the trail artifacts in #58.
	// Disable blending; the frag shader's `discard` already handles the
	// colorkey path.
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glViewport(0, 0, tgt->GetWidth(), tgt->GetHeight());

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glEnable(GL_DEPTH_TEST);
	glBindVertexArray(0);
	glUseProgram(0);

	if (tgt) {
		tgt->UnbindFBO();
	} else {
		glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)bbFBO);
		glViewport(bbVp[0], bbVp[1], bbVp[2], bbVp[3]);
	}
}

// ============================================================================
// Surface blitting callbacks (Phase 1A)
// ============================================================================

bool OGLClient::clbkBlt(SURFHANDLE tgt, DWORD tgtx, DWORD tgty, SURFHANDLE src, DWORD flag) const
{
	if (!src) return false;
	OGLSurface *srcS = (OGLSurface*)src;
	OGLSurface *tgtS = (OGLSurface*)tgt; // tgt==NULL → backbuffer (handled by BlitQuad)
	BlitQuad(tgtS, tgtx, tgty, srcS->GetWidth(), srcS->GetHeight(),
	         srcS, 0, 0, srcS->GetWidth(), srcS->GetHeight(), flag);
	return true;
}

bool OGLClient::clbkBlt(SURFHANDLE tgt, DWORD tgtx, DWORD tgty, SURFHANDLE src,
                         DWORD srcx, DWORD srcy, DWORD w, DWORD h, DWORD flag) const
{
	if (!src) return false;
	OGLSurface *srcS = (OGLSurface*)src;
	OGLSurface *tgtS = (OGLSurface*)tgt;

	// Self-blit fast path (see issue #62): when copying within the same
	// surface, both sides share one orientation regardless of whether the
	// content was originally BMP-uploaded or FBO-rendered. BlitQuad's NDC
	// math assumes the dst is OGL-native (NDC y=+1 is visual top), which
	// is correct for a pure FBO surface but wrong for a hybrid surface
	// like HUD::hudTex that ships with a BMP-uploaded font atlas AND is
	// later sampled with BMP-convention UVs by the HUD mesh. The rasterised
	// glyphs landed in texture memory rows 0..dy instead of rows
	// tgty..tgty+dy, so the mesh drew the pre-baked default atlas tile
	// every time regardless of the blit. Using glBlitFramebuffer for the
	// self-blit case gives a raw pixel copy in absolute texture coordinates
	// and side-steps the NDC/UV convention question entirely.
	if (tgtS && srcS == tgtS && !(flag & BLT_SRCCOLORKEY)) {
		return BlitFramebufferSelf(srcS, srcx, srcy, tgtx, tgty, w, h);
	}

	BlitQuad(tgtS, tgtx, tgty, w, h, srcS, srcx, srcy, w, h, flag);
	return true;
}

bool OGLClient::BlitFramebufferSelf(OGLSurface *surf,
                                     DWORD srcx, DWORD srcy,
                                     DWORD tgtx, DWORD tgty,
                                     DWORD w, DWORD h) const
{
	if (!surf) return false;
	GLuint fbo = surf->EnsureFBO();
	if (!fbo) return false;

	GLint prevReadFBO = 0, prevDrawFBO = 0;
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevReadFBO);
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevDrawFBO);

	glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);

	// Pixel coordinates are in texture-memory space (row 0 = first row
	// uploaded by glTexImage2D = visual BMP top). glBlitFramebuffer treats
	// the FBO like a viewport, and since the FBO attachment maps y=0 to
	// texture row 0, feeding BMP-convention tgtx/tgty directly lands the
	// pixels at the expected rows. Non-overlapping src/dst rects so the
	// same-FBO read+draw binding is well-defined.
	glBlitFramebuffer(
		(GLint)srcx, (GLint)srcy, (GLint)(srcx + w), (GLint)(srcy + h),
		(GLint)tgtx, (GLint)tgty, (GLint)(tgtx + w), (GLint)(tgty + h),
		GL_COLOR_BUFFER_BIT, GL_NEAREST);

	glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)prevReadFBO);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)prevDrawFBO);
	return true;
}

bool OGLClient::clbkScaleBlt(SURFHANDLE tgt, DWORD tgtx, DWORD tgty, DWORD tgtw, DWORD tgth,
                              SURFHANDLE src, DWORD srcx, DWORD srcy, DWORD srcw, DWORD srch,
                              DWORD flag) const
{
	if (!src) return false;
	OGLSurface *srcS = (OGLSurface*)src;
	OGLSurface *tgtS = (OGLSurface*)tgt;
	BlitQuad(tgtS, tgtx, tgty, tgtw, tgth, srcS, srcx, srcy, srcw, srch, flag);
	return true;
}

bool OGLClient::clbkFillSurface(SURFHANDLE surf, DWORD col) const
{
	if (!surf) return false;
	((OGLSurface*)surf)->Fill(col);
	return true;
}

bool OGLClient::clbkFillSurface(SURFHANDLE surf, DWORD tgtx, DWORD tgty, DWORD w, DWORD h, DWORD col) const
{
	if (!surf) return false;
	((OGLSurface*)surf)->Fill(tgtx, tgty, w, h, col);
	return true;
}

int OGLClient::clbkBeginBltGroup(SURFHANDLE tgt)
{
	if (m_bltGroupTgt) return -2; // already in a group
	m_bltGroupTgt = tgt;
	return 0;
}

int OGLClient::clbkEndBltGroup()
{
	if (!m_bltGroupTgt) return -2;
	m_bltGroupTgt = nullptr;
	return 0;
}

// ============================================================================
// 2D Panel rendering (Phase 1C)
// ============================================================================

void OGLClient::clbkRender2DPanel(SURFHANDLE *hSurf, MESHHANDLE hMesh, MATRIX3 *T, bool additive)
{
	clbkRender2DPanel(hSurf, hMesh, T, 1.0f, additive);
}

void OGLClient::clbkRender2DPanel(SURFHANDLE *hSurf, MESHHANDLE hMesh, MATRIX3 *T, float alpha, bool additive)
{
	if (!hMesh || !m_panel2dShader || !T) return;

	DWORD nGrp = oapiMeshGroupCount(hMesh);
	if (nGrp == 0) return;

	glUseProgram(m_panel2dShader);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	if (additive)
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	else
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Upload transform as a column-major mat3 with only the diagonal scale
	// (m11, m22) and translation column (m13, m23) — matching the D3D9
	// client (D3D9Client.cpp:1948) which reads only those four elements.
	// DefaultPanel builds `transf = {1,-0.5,0, 0,1,-0.5, 0,0,1}`: the -0.5
	// in m12/m23 positions is ignored on Windows but, applied as a full
	// mat3, would shear the generic cockpit (the tilted rendering tracked
	// in #54). Zeroing the off-diagonal shear reproduces Win32 behaviour.
	// Layout: shader computes `uTransform * vec3(x, y, 1)`, so we need
	// columns (m11, 0, 0), (0, m22, 0), (m13, m23, 1).
	float mat[9] = {
		(float)T->m11, 0.0f,          0.0f,
		0.0f,          (float)T->m22, 0.0f,
		(float)T->m13, (float)T->m23, 1.0f
	};
	glUniformMatrix3fv(m_shaderMgr->GetUniformLoc(m_panel2dShader, "uTransform"), 1, GL_FALSE, mat);
	glUniform2f(m_shaderMgr->GetUniformLoc(m_panel2dShader, "uViewport"), (float)m_viewW, (float)m_viewH);
	glUniform1f(m_shaderMgr->GetUniformLoc(m_panel2dShader, "uAlpha"), alpha);

	for (DWORD g = 0; g < nGrp; g++) {
		MESHGROUPEX *grp = oapiMeshGroupEx(hMesh, g);
		if (!grp || !grp->nVtx || !grp->nIdx) continue;

		// UsrFlag bit 2 is Orbiter's "skip this group in the 2D panel pass"
		// marker — typically used for groups that belong to a different
		// panel orientation or are geometry placeholders. Matches D3D9Client
		// (D3D9Client.cpp:1963).
		if (grp->UsrFlag & 2) continue;

		// Resolve the group's texture. Three cases, matching D3D9's ordering:
		//   1. TexIdx is in [TEXIDX_MFD0, TEXIDX_MFD0+MAXMFD) → the group is
		//      an MFD display slot; bind the MFD's painted surface. Without
		//      this, MFD panels render as uninitialised/garbage indexes.
		//   2. hSurf provided → lookup into the caller's texture array.
		//   3. hSurf null → fall back to the mesh's own texture list, with
		//      oapiGetTextureHandle's 1-based indexing.
		DWORD texIdx = grp->TexIdx;
		SURFHANDLE hTex = nullptr;
		if (texIdx >= TEXIDX_MFD0 && texIdx < TEXIDX_MFD0 + MAXMFD) {
			int mfdidx = (int)(texIdx - TEXIDX_MFD0);
			hTex = GetMFDSurface(mfdidx);
		} else if (hSurf && texIdx != (DWORD)-1) {
			hTex = hSurf[texIdx];
		} else if (texIdx != (DWORD)-1) {
			hTex = oapiGetTextureHandle(hMesh, texIdx + 1);
		}

		// MFD textures are painted by sketchpad into an FBO, which stores the
		// visual top at OGL v=1. The defpanel mesh's tv values assume D3D's
		// "v=0 at top" convention, so we flip tv for MFD groups to keep the
		// Surface MFD tape/horizon labels upright (#58). We *only* flip for
		// MFD slots: the HUD font atlas (hudTex) is loaded with
		// OAPISURFACE_RENDERTARGET too — but its content is a BMP, not an
		// FBO render — so attrib-based detection would wrongly flip the
		// HUD pitch-ladder glyphs (they came out as "ORBIT" labels because
		// the font atlas was being sampled vertically inverted).
		const bool isMFD = (texIdx >= TEXIDX_MFD0 && texIdx < TEXIDX_MFD0 + MAXMFD);
		const bool flipV = isMFD;
		if (hTex) {
			OGLSurface *texSurf = (OGLSurface*)hTex;
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, texSurf->GetTexture());
			glUniform1i(m_shaderMgr->GetUniformLoc(m_panel2dShader, "uTexture"), 0);
		}

		// Upload vertex data (pos.x, pos.y as vec2 + tu, tv as vec2)
		// NTVERTEX layout: x,y,z, nx,ny,nz, tu,tv — we take x,y and tu,tv
		GLuint tmpVAO, tmpVBO, tmpEBO;
		glGenVertexArrays(1, &tmpVAO);
		glGenBuffers(1, &tmpVBO);
		glGenBuffers(1, &tmpEBO);
		glBindVertexArray(tmpVAO);

		// Build interleaved 2D vertex data: [x, y, u, v]
		std::vector<float> vdata(grp->nVtx * 4);
		for (DWORD i = 0; i < grp->nVtx; i++) {
			vdata[i * 4 + 0] = grp->Vtx[i].x;
			vdata[i * 4 + 1] = grp->Vtx[i].y;
			vdata[i * 4 + 2] = grp->Vtx[i].tu;
			vdata[i * 4 + 3] = flipV ? (1.0f - grp->Vtx[i].tv) : grp->Vtx[i].tv;
		}
		glBindBuffer(GL_ARRAY_BUFFER, tmpVBO);
		glBufferData(GL_ARRAY_BUFFER, vdata.size() * sizeof(float), vdata.data(), GL_STREAM_DRAW);

		std::vector<unsigned int> indices(grp->nIdx);
		for (DWORD i = 0; i < grp->nIdx; i++) indices[i] = (unsigned int)grp->Idx[i];
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, tmpEBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STREAM_DRAW);

		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
		glEnableVertexAttribArray(1);

		glDrawElements(GL_TRIANGLES, (GLsizei)grp->nIdx, GL_UNSIGNED_INT, 0);

		glBindVertexArray(0);
		glDeleteVertexArrays(1, &tmpVAO);
		glDeleteBuffers(1, &tmpVBO);
		glDeleteBuffers(1, &tmpEBO);
	}

	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);
}

// ============================================================================
// M1 self-test — RGBA8 RT round-trip (create/clear/readback/unbind)
// ============================================================================

void OGLClient::RunM1SelfTest()
{
	constexpr int W = 256, H = 256;
	OGLSurface *surf = new OGLSurface();
	if (!surf->CreateEx(W, H, OAPISURFACE_RENDERTARGET | OAPISURFACE_MIPMAPS, 0, false)) {
		fprintf(stderr, "[OGLClient] M1 self-test: CreateEx FAILED\n");
		surf->Release();
		return;
	}

	GLuint fbo = surf->EnsureFBO();
	if (!fbo) {
		fprintf(stderr, "[OGLClient] M1 self-test: EnsureFBO FAILED\n");
		surf->Release();
		return;
	}

	// Known clear colour.
	constexpr float rF = 0.25f, gF = 0.50f, bF = 0.75f, aF = 1.00f;
	unsigned char rgba[4] = { 0, 0, 0, 0 };

	{
		FBOBinder guard(fbo, W, H);
		glClearColor(rF, gF, bF, aF);
		glClear(GL_COLOR_BUFFER_BIT);
		glReadPixels(W / 2, H / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
	}

	auto near8 = [](unsigned char got, float wantF) {
		int want = (int)(wantF * 255.0f + 0.5f);
		return std::abs((int)got - want) <= 2;
	};
	bool ok = near8(rgba[0], rF) && near8(rgba[1], gF) &&
	          near8(rgba[2], bF) && near8(rgba[3], aF);

	fprintf(stderr,
	        "[OGLClient] M1 self-test: fill=(%.2f,%.2f,%.2f,%.2f) readback=(0x%02x,0x%02x,0x%02x,0x%02x) %s\n",
	        rF, gF, bF, aF, rgba[0], rgba[1], rgba[2], rgba[3],
	        ok ? "PASS" : "FAIL");

	surf->Release();
}

// ============================================================================
// Mesh manipulation (Phase 1D)
// ============================================================================

bool OGLClient::clbkSetMeshTexture(DEVMESHHANDLE hMesh, DWORD texidx, SURFHANDLE tex)
{
	// A proper device-mesh texture override requires a per-DEVMESHHANDLE
	// texture table (tracked by OGLvVessel, not the registry). That lands
	// with M17 when the Sketchpad/MFD pipeline gains a distinct device mesh.
	// For M2 we stay "not supported" so callers fall back to the template's
	// own texture slot.
	(void)hMesh; (void)texidx; (void)tex;
	return false;
}

int OGLClient::clbkSetMeshMaterial(DEVMESHHANDLE hMesh, DWORD matidx, const MATERIAL *mat)
{
	// Material plumbing through a UBO is M3 territory — until then we advertise
	// "not supported" so vessel modules fall back to their own legacy path.
	// We do *not* invalidate: nothing on the GPU side changed.
	(void)hMesh; (void)matidx; (void)mat;
	return 2;
}

int OGLClient::clbkMeshMaterial(DEVMESHHANDLE hMesh, DWORD matidx, MATERIAL *mat)
{
	(void)hMesh; (void)matidx; (void)mat;
	return 2;
}

bool OGLClient::clbkSetMeshProperty(DEVMESHHANDLE hMesh, DWORD property, DWORD value)
{
	(void)hMesh; (void)property; (void)value;
	return false;
}

int OGLClient::clbkGetMeshGroup(DEVMESHHANDLE hMesh, DWORD grpidx, GROUPREQUESTSPEC *grs)
{
	if (!hMesh || !grs) return -1;
	MESHGROUPEX *grp = oapiMeshGroupEx((MESHHANDLE)hMesh, grpidx);
	if (!grp) return -1;

	if (grs->Vtx && grs->nVtx > 0) {
		DWORD n = std::min(grs->nVtx, grp->nVtx);
		memcpy(grs->Vtx, grp->Vtx, n * sizeof(NTVERTEX));
		grs->nVtx = n;
	}
	if (grs->Idx && grs->nIdx > 0) {
		DWORD n = std::min(grs->nIdx, grp->nIdx);
		memcpy(grs->Idx, grp->Idx, n * sizeof(WORD));
		grs->nIdx = n;
	}
	grs->MtrlIdx = grp->MtrlIdx;
	grs->TexIdx = grp->TexIdx;
	return 0;
}

int OGLClient::clbkEditMeshGroup(DEVMESHHANDLE hMesh, DWORD grpidx, GROUPEDITSPEC *ges)
{
	if (!hMesh || !ges) return -1;
	MESHGROUPEX *grp = oapiMeshGroupEx((MESHHANDLE)hMesh, grpidx);
	if (!grp) return -1;

	if (ges->Vtx && ges->nVtx > 0) {
		DWORD n = std::min(ges->nVtx, grp->nVtx);
		if (ges->vIdx) {
			for (DWORD i = 0; i < n; i++) {
				DWORD vi = ges->vIdx[i];
				if (vi < grp->nVtx) {
					if (ges->flags & GRPEDIT_VTXCRD) {
						grp->Vtx[vi].x = ges->Vtx[i].x;
						grp->Vtx[vi].y = ges->Vtx[i].y;
						grp->Vtx[vi].z = ges->Vtx[i].z;
					}
					if (ges->flags & GRPEDIT_VTXNML) {
						grp->Vtx[vi].nx = ges->Vtx[i].nx;
						grp->Vtx[vi].ny = ges->Vtx[i].ny;
						grp->Vtx[vi].nz = ges->Vtx[i].nz;
					}
					if (ges->flags & GRPEDIT_VTXTEX) {
						grp->Vtx[vi].tu = ges->Vtx[i].tu;
						grp->Vtx[vi].tv = ges->Vtx[i].tv;
					}
				}
			}
		} else {
			for (DWORD i = 0; i < n && i < grp->nVtx; i++) {
				if (ges->flags & GRPEDIT_VTXCRD) {
					grp->Vtx[i].x = ges->Vtx[i].x;
					grp->Vtx[i].y = ges->Vtx[i].y;
					grp->Vtx[i].z = ges->Vtx[i].z;
				}
				if (ges->flags & GRPEDIT_VTXNML) {
					grp->Vtx[i].nx = ges->Vtx[i].nx;
					grp->Vtx[i].ny = ges->Vtx[i].ny;
					grp->Vtx[i].nz = ges->Vtx[i].nz;
				}
				if (ges->flags & GRPEDIT_VTXTEX) {
					grp->Vtx[i].tu = ges->Vtx[i].tu;
					grp->Vtx[i].tv = ges->Vtx[i].tv;
				}
			}
		}
	}
	// Edit affected this group's vertex data: mark the cache entry dirty
	// so the next Acquire rebuilds the VBO/EBO tuple from the fresh
	// MESHGROUPEX contents.
	MeshRegistry::Instance().InvalidateGroup((MESHHANDLE)hMesh, grpidx);
	return 0;
}

MESHHANDLE OGLClient::clbkGetMesh(VISHANDLE vis, UINT idx)
{
	// vis is currently unused — we don't have a vis→mesh mapping yet
	return nullptr;
}

// ============================================================================
// Visual events (Phase 1E)
// ============================================================================

int OGLClient::clbkVisEvent(OBJHANDLE hObj, VISHANDLE vis, DWORD msg, DWORD_PTR context)
{
	return 1;
}

// ============================================================================
// Particle streams (Phase 5A)
// ============================================================================

oapi::ParticleStream *OGLClient::clbkCreateParticleStream(PARTICLESTREAMSPEC *pss)
{
	return new OGLParticleStream(this, pss, m_shaderMgr);
}

oapi::ParticleStream *OGLClient::clbkCreateExhaustStream(PARTICLESTREAMSPEC *pss,
	OBJHANDLE hVessel, const double *lvl, const VECTOR3 *ref, const VECTOR3 *dir)
{
	return new OGLExhaustStream(this, pss, m_shaderMgr, hVessel, lvl, ref, dir);
}

oapi::ParticleStream *OGLClient::clbkCreateReentryStream(PARTICLESTREAMSPEC *pss,
	OBJHANDLE hVessel)
{
	return new OGLReentryStream(this, pss, m_shaderMgr, hVessel);
}

// ============================================================================
// Screen annotations (Phase 5C)
// ============================================================================

oapi::ScreenAnnotation *OGLClient::clbkCreateAnnotation()
{
	return new OGLAnnotation(this);
}

// ============================================================================
// Additional surface methods (Phase 9A)
// ============================================================================

SURFHANDLE OGLClient::clbkLoadSurface(const char *fname, DWORD attrib, bool bPath)
{
	// Load texture and wrap in surface with specified attributes
	SURFHANDLE h = clbkLoadTexture(fname, 0);
	if (h && attrib) {
		OGLSurface *s = (OGLSurface*)h;
		// WrapTexture() set m_attrib to OAPISURFACE_TEXTURE. Merge the
		// caller's requested attribs so EnsureFBO() attaches the depth
		// renderbuffer (required for RT/SKETCHPAD use). A previously-
		// wrapped file-loaded GL texture may still not be color-renderable
		// on macOS GL 4.1 — EnsureFBO() will log once and fail silently
		// if so; clbkGetSketchpad() then rejects further attempts.
		s->AddAttrib(attrib);
		if (attrib & (OAPISURFACE_RENDERTARGET | OAPISURFACE_SKETCHPAD))
			s->EnsureFBO();
	}
	return h;
}

SURFHANDLE OGLClient::clbkCreateTexture(DWORD w, DWORD h)
{
	return clbkCreateSurfaceEx(w, h, OAPISURFACE_TEXTURE);
}

bool OGLClient::clbkSaveSurfaceToImage(SURFHANDLE surf, const char *fname,
	oapi::ImageFileFormat fmt, float quality)
{
	if (!surf || !fname) return false;
	OGLSurface *s = (OGLSurface*)surf;

	// Read pixels from texture via FBO
	DWORD w = s->GetWidth(), h = s->GetHeight();
	std::vector<unsigned char> pixels(w * h * 4);

	s->BindFBO();
	glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
	s->UnbindFBO();

	// Write as BMP (simple, always works)
	FILE *f = fopen(fname, "wb");
	if (!f) return false;

	int rowSize = w * 3;
	int pad = (4 - rowSize % 4) % 4;
	int imgSize = (rowSize + pad) * h;
	unsigned char bmpFileHeader[14] = {'B','M', 0,0,0,0, 0,0,0,0, 54,0,0,0};
	int fileSize = 54 + imgSize;
	bmpFileHeader[2] = fileSize; bmpFileHeader[3] = fileSize >> 8;
	bmpFileHeader[4] = fileSize >> 16; bmpFileHeader[5] = fileSize >> 24;
	fwrite(bmpFileHeader, 1, 14, f);

	unsigned char bmpInfoHeader[40] = {};
	bmpInfoHeader[0] = 40;
	bmpInfoHeader[4] = w; bmpInfoHeader[5] = w >> 8;
	bmpInfoHeader[6] = w >> 16; bmpInfoHeader[7] = w >> 24;
	bmpInfoHeader[8] = h; bmpInfoHeader[9] = h >> 8;
	bmpInfoHeader[10] = h >> 16; bmpInfoHeader[11] = h >> 24;
	bmpInfoHeader[12] = 1; bmpInfoHeader[14] = 24;
	fwrite(bmpInfoHeader, 1, 40, f);

	unsigned char padBytes[3] = {0,0,0};
	for (DWORD y = 0; y < h; y++) {
		for (DWORD x = 0; x < w; x++) {
			int idx = (y * w + x) * 4;
			unsigned char bgr[3] = {pixels[idx+2], pixels[idx+1], pixels[idx]};
			fwrite(bgr, 1, 3, f);
		}
		if (pad) fwrite(padBytes, 1, pad, f);
	}
	fclose(f);
	return true;
}

// ============================================================================
// Splash screen (Phase 9A)
// ============================================================================

bool OGLClient::clbkSplashLoadMsg(const char *msg, int line)
{
	if (msg)
		fprintf(stderr, "[Orbiter] %s\n", msg);
	return true;
}

void OGLClient::clbkSetSplashScreen(const char *fname, DWORD textCol)
{
	// Splash screen not yet implemented — sim starts directly
}

// ============================================================================
// Mesh persistence and extended materials (Phase 9A)
// ============================================================================

void OGLClient::clbkStoreMeshPersistent(MESHHANDLE hMesh, const char *fname)
{
	// Store mesh association for later retrieval — currently a no-op
}

int OGLClient::clbkSetMeshMaterialEx(DEVMESHHANDLE hMesh, DWORD matidx, MatProp prp, const oapi::FVECTOR4 *in)
{
	if (!in) return 2;
	int rc = MaterialStore::Instance().Set(hMesh, matidx, prp, *in);
	if (rc == 0)
		MeshRegistry::Instance().InvalidateMesh((MESHHANDLE)hMesh);
	return rc;
}

int OGLClient::clbkMeshMaterialEx(DEVMESHHANDLE hMesh, DWORD matidx, MatProp prp, oapi::FVECTOR4 *out)
{
	if (!out) return 2;
	return MaterialStore::Instance().Get(hMesh, matidx, prp, *out);
}

// ============================================================================
// Notification hooks (Phase 9A)
// ============================================================================

void OGLClient::clbkOptionChanged(DWORD cat, DWORD item)
{
	// Respond to runtime config changes — can update post-processing, etc.
}

void OGLClient::clbkPreOpenPopup()
{
	// Called before popup dialogs — can pause rendering if needed
}

bool OGLClient::clbkUseLaunchpadVideoTab() const
{
	return false; // macOS uses ImGui launchpad, not Win32 video tab
}

// ============================================================================
// OGL Font / Pen / Brush / Sketchpad — see OGLSketchpad.{h,cpp}
// ============================================================================
// The concrete classes live in OGLSketchpad.cpp so every MFD draw hits the
// bound surface's FBO via a dedicated 2D shader. OGLClient just forwards
// the clbk factory methods.

// --- clbk implementations ---

oapi::Font *OGLClient::clbkCreateFont(int height, bool prop, const char *face,
	FontStyle style, int orientation) const
{
	return new OGLFont(height, prop, face, style, orientation);
}

void OGLClient::clbkReleaseFont(oapi::Font *font) const { delete font; }

oapi::Font *OGLClient::clbkCreateFontEx(int height, char *face, int width, int weight,
	FontStyle style, float spacing) const
{
	bool prop = (width == 0); // proportional if width not specified
	return new OGLFont(height, prop, face, style, 0);
}

oapi::Pen *OGLClient::clbkCreatePen(int style, int width, DWORD col) const {
	return new OGLPen(style, width, col);
}
void OGLClient::clbkReleasePen(oapi::Pen *pen) const { delete pen; }

oapi::Brush *OGLClient::clbkCreateBrush(DWORD col) const {
	return new OGLBrush(col);
}
void OGLClient::clbkReleaseBrush(oapi::Brush *brush) const { delete brush; }

oapi::Sketchpad *OGLClient::clbkGetSketchpad(SURFHANDLE surf) {
	if (!surf) return nullptr;
	if (!m_imguiInitialized) return nullptr;
	if (!ImGui::GetCurrentContext()) return nullptr;
	// Don't gate on IsBuilt(): the dynamic atlas flips it to false transiently
	// while baking new glyphs. Text() handles not-ready atlas; non-text ops
	// don't need it.
	if (!ImGui::GetIO().Fonts) return nullptr;
	// Target surface must be render-target capable. Pure-texture surfaces
	// (OAPISURFACE_TEXTURE without any RT bit, typically loaded via
	// clbkLoadTexture) can't accept draw commands — attempting EnsureFBO on
	// a WrapTexture-wrapped GL texture fails with MISSING_ATTACHMENT and
	// floods stderr. Reject them up-front; callers must handle the nullptr.
	OGLSurface *s = (OGLSurface*)surf;
	const DWORD rtMask = OAPISURFACE_RENDERTARGET | OAPISURFACE_RENDER3D | OAPISURFACE_SKETCHPAD;
	if (!(s->GetAttrib() & rtMask)) return nullptr;
	if (!s->EnsureFBO()) return nullptr;
	OGLSketchpad::InitShared(m_shaderMgr);
	return new OGLSketchpad(surf, m_shaderMgr);
}

void OGLClient::clbkReleaseSketchpad(oapi::Sketchpad *sp) { delete sp; }

void OGLClient::SetSDLWindow(SDL_Window *w, SDL_GLContext c) { m_sdlWindow = w; m_sdlContext = c; }

} // namespace ogl

#endif // !_WIN32
