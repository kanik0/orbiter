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
	  m_shaderMgr(nullptr), m_scene(nullptr),
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

	// Initialize blit/panel resources
	InitBlitResources();

	fprintf(stderr, "[OGLClient] Scene initialized\n");
	return (HWND)m_sdlWindow;
}

void OGLClient::clbkDestroyRenderWindow(bool fastclose)
{
	fprintf(stderr, "[OGLClient] clbkDestroyRenderWindow\n");

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

// Core blit helper: renders a textured quad from src rect to tgt rect via FBO
void OGLClient::BlitQuad(OGLSurface *tgt, DWORD tgtx, DWORD tgty, DWORD tgtw, DWORD tgth,
                          OGLSurface *src, DWORD srcx, DWORD srcy, DWORD srcw, DWORD srch,
                          DWORD flag) const
{
	if (!m_blitShader || !m_blitVAO || !src || !tgt) return;

	// Compute source UV coords
	float sw = (float)src->GetWidth(), sh = (float)src->GetHeight();
	float u0 = (float)srcx / sw, v0 = (float)srcy / sh;
	float u1 = (float)(srcx + srcw) / sw, v1 = (float)(srcy + srch) / sh;

	// Compute target NDC coords within the FBO viewport
	float tw = (float)tgt->GetWidth(), th = (float)tgt->GetHeight();
	float x0 = (float)tgtx / tw * 2.0f - 1.0f;
	float y0 = 1.0f - (float)(tgty + tgth) / th * 2.0f; // flip y for OpenGL
	float x1 = (float)(tgtx + tgtw) / tw * 2.0f - 1.0f;
	float y1 = 1.0f - (float)tgty / th * 2.0f;

	float verts[] = {
		x0, y0, u0, v1,
		x1, y0, u1, v1,
		x0, y1, u0, v0,
		x1, y1, u1, v0,
	};

	// Bind target FBO
	tgt->BindFBO();

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

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DEPTH_TEST);
	glViewport(0, 0, tgt->GetWidth(), tgt->GetHeight());

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glBindVertexArray(0);
	glUseProgram(0);

	tgt->UnbindFBO();
}

// ============================================================================
// Surface blitting callbacks (Phase 1A)
// ============================================================================

bool OGLClient::clbkBlt(SURFHANDLE tgt, DWORD tgtx, DWORD tgty, SURFHANDLE src, DWORD flag) const
{
	if (!src) return false;
	OGLSurface *srcS = (OGLSurface*)src;
	OGLSurface *tgtS = tgt ? (OGLSurface*)tgt : nullptr;
	if (!tgtS) return false; // TODO: blit to backbuffer when tgt==NULL
	BlitQuad(tgtS, tgtx, tgty, srcS->GetWidth(), srcS->GetHeight(),
	         srcS, 0, 0, srcS->GetWidth(), srcS->GetHeight(), flag);
	return true;
}

bool OGLClient::clbkBlt(SURFHANDLE tgt, DWORD tgtx, DWORD tgty, SURFHANDLE src,
                         DWORD srcx, DWORD srcy, DWORD w, DWORD h, DWORD flag) const
{
	if (!src) return false;
	OGLSurface *srcS = (OGLSurface*)src;
	OGLSurface *tgtS = tgt ? (OGLSurface*)tgt : nullptr;
	if (!tgtS) return false;
	BlitQuad(tgtS, tgtx, tgty, w, h, srcS, srcx, srcy, w, h, flag);
	return true;
}

bool OGLClient::clbkScaleBlt(SURFHANDLE tgt, DWORD tgtx, DWORD tgty, DWORD tgtw, DWORD tgth,
                              SURFHANDLE src, DWORD srcx, DWORD srcy, DWORD srcw, DWORD srch,
                              DWORD flag) const
{
	if (!src) return false;
	OGLSurface *srcS = (OGLSurface*)src;
	OGLSurface *tgtS = tgt ? (OGLSurface*)tgt : nullptr;
	if (!tgtS) return false;
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
	if (!hMesh || !m_panel2dShader) return;

	DWORD nGrp = oapiMeshGroupCount(hMesh);
	if (nGrp == 0) return;

	glUseProgram(m_panel2dShader);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	if (additive)
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	else
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Upload transform as mat3 uniform
	float mat[9] = {
		(float)T->m11, (float)T->m12, (float)T->m13,
		(float)T->m21, (float)T->m22, (float)T->m23,
		(float)T->m31, (float)T->m32, (float)T->m33
	};
	glUniformMatrix3fv(m_shaderMgr->GetUniformLoc(m_panel2dShader, "uTransform"), 1, GL_FALSE, mat);
	glUniform2f(m_shaderMgr->GetUniformLoc(m_panel2dShader, "uViewport"), (float)m_viewW, (float)m_viewH);
	glUniform1f(m_shaderMgr->GetUniformLoc(m_panel2dShader, "uAlpha"), alpha);

	for (DWORD g = 0; g < nGrp; g++) {
		MESHGROUPEX *grp = oapiMeshGroupEx(hMesh, g);
		if (!grp || !grp->nVtx || !grp->nIdx) continue;

		// Bind the texture for this group
		DWORD texIdx = grp->TexIdx;
		SURFHANDLE hTex = nullptr;
		if (texIdx != (DWORD)-1 && hSurf)
			hTex = hSurf[texIdx];

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
			vdata[i * 4 + 3] = grp->Vtx[i].tv;
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
// Mesh manipulation (Phase 1D)
// ============================================================================

bool OGLClient::clbkSetMeshTexture(DEVMESHHANDLE hMesh, DWORD texidx, SURFHANDLE tex)
{
	// Device meshes are not yet tracked separately; stub for now
	return false;
}

int OGLClient::clbkSetMeshMaterial(DEVMESHHANDLE hMesh, DWORD matidx, const MATERIAL *mat)
{
	return 2; // not yet supported
}

int OGLClient::clbkMeshMaterial(DEVMESHHANDLE hMesh, DWORD matidx, MATERIAL *mat)
{
	return 2; // not yet supported
}

bool OGLClient::clbkSetMeshProperty(DEVMESHHANDLE hMesh, DWORD property, DWORD value)
{
	return false; // not yet supported
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
	// TODO: invalidate GPU mesh cache for this mesh handle
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
	// Accept all visual events — vessel modules expect this to return 1
	// to confirm the graphics client is handling visuals.
	// Specific message handling will be added as the visual system matures.
	return 1;
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
