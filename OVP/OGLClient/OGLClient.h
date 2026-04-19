// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// OpenGL Graphics Client for Orbiter on macOS/Linux
// Implements the oapi::GraphicsClient interface using OpenGL 4.1 + SDL2

#ifndef __OGLCLIENT_H
#define __OGLCLIENT_H

#include "GraphicsAPI.h"

#ifndef _WIN32
#include <SDL.h>
#include <OpenGL/gl3.h>
#include <string>
#endif

namespace ogl {

class ShaderMgr;
class OGLScene;
class OGLSurface;
class OGLPostProcess;

class OGLClient : public oapi::GraphicsClient {
public:
	OGLClient(HINSTANCE hInstance);
	~OGLClient();

	// === Pure virtual methods (MUST implement) ===

	bool clbkFullscreenMode() const override;
	void clbkGetViewportSize(DWORD *width, DWORD *height) const override;
	bool clbkGetRenderParam(DWORD prm, DWORD *value) const override;
	void clbkRenderScene() override;

	// ImGui integration
	void clbkImGuiInit() override;
	void clbkImGuiShutdown() override;
	void clbkImGuiNewFrame() override;
	void clbkImGuiRenderDrawData() override;
	uint64_t clbkImGuiSurfaceTexture(SURFHANDLE surf) override;
	void clbkRenderImGuiPlugins() override;
	bool clbkSaveScreenshot(const char *path, int w = 0, int h = 0) override;

	// === Session lifecycle ===

	HWND clbkCreateRenderWindow() override;
	void clbkDestroyRenderWindow(bool fastclose) override;
	bool clbkDisplayFrame() override;
	void clbkUpdate(bool running) override;
	void clbkPostCreation() override;
	void clbkCloseSession(bool fastclose) override;

	// === Texture/Surface management ===

	SURFHANDLE clbkLoadTexture(const char *fname, DWORD flags = 0) override;
	void clbkReleaseTexture(SURFHANDLE hTex) override;
	bool clbkReleaseSurface(SURFHANDLE surf) override;
	SURFHANDLE clbkCreateSurfaceEx(DWORD w, DWORD h, DWORD attrib) override;
	SURFHANDLE clbkCreateSurface(DWORD w, DWORD h, SURFHANDLE hTemplate = NULL) override;
	bool clbkGetSurfaceSize(SURFHANDLE surf, DWORD *w, DWORD *h) override;
	void clbkIncrSurfaceRef(SURFHANDLE surf) override;
	bool clbkSetSurfaceColourKey(SURFHANDLE surf, DWORD ckey) override;
	bool clbkClearSurfaceColourKey(SURFHANDLE surf) override;
	DWORD clbkGetDeviceColour(BYTE r, BYTE g, BYTE b) override;

	// === Surface blitting ===

	bool clbkBlt(SURFHANDLE tgt, DWORD tgtx, DWORD tgty, SURFHANDLE src, DWORD flag = 0) const override;
	bool clbkBlt(SURFHANDLE tgt, DWORD tgtx, DWORD tgty, SURFHANDLE src, DWORD srcx, DWORD srcy, DWORD w, DWORD h, DWORD flag = 0) const override;
	bool clbkScaleBlt(SURFHANDLE tgt, DWORD tgtx, DWORD tgty, DWORD tgtw, DWORD tgth, SURFHANDLE src, DWORD srcx, DWORD srcy, DWORD srcw, DWORD srch, DWORD flag = 0) const override;
	bool clbkFillSurface(SURFHANDLE surf, DWORD col) const override;
	bool clbkFillSurface(SURFHANDLE surf, DWORD tgtx, DWORD tgty, DWORD w, DWORD h, DWORD col) const override;
	int clbkBeginBltGroup(SURFHANDLE tgt) override;
	int clbkEndBltGroup() override;

	// === 2D Panel rendering ===

	void clbkRender2DPanel(SURFHANDLE *hSurf, MESHHANDLE hMesh, MATRIX3 *T, bool additive = false) override;
	void clbkRender2DPanel(SURFHANDLE *hSurf, MESHHANDLE hMesh, MATRIX3 *T, float alpha, bool additive = false) override;

	// === Mesh manipulation ===

	bool clbkSetMeshTexture(DEVMESHHANDLE hMesh, DWORD texidx, SURFHANDLE tex) override;
	int clbkSetMeshMaterial(DEVMESHHANDLE hMesh, DWORD matidx, const MATERIAL *mat) override;
	int clbkMeshMaterial(DEVMESHHANDLE hMesh, DWORD matidx, MATERIAL *mat) override;
	bool clbkSetMeshProperty(DEVMESHHANDLE hMesh, DWORD property, DWORD value) override;
	int clbkGetMeshGroup(DEVMESHHANDLE hMesh, DWORD grpidx, GROUPREQUESTSPEC *grs) override;
	int clbkEditMeshGroup(DEVMESHHANDLE hMesh, DWORD grpidx, GROUPEDITSPEC *ges) override;
	MESHHANDLE clbkGetMesh(VISHANDLE vis, UINT idx) override;

	// === Visual events ===

	int clbkVisEvent(OBJHANDLE hObj, VISHANDLE vis, DWORD msg, DWORD_PTR context) override;

	// === Particle streams ===

	oapi::ParticleStream *clbkCreateParticleStream(PARTICLESTREAMSPEC *pss) override;
	oapi::ParticleStream *clbkCreateExhaustStream(PARTICLESTREAMSPEC *pss,
		OBJHANDLE hVessel, const double *lvl, const VECTOR3 *ref, const VECTOR3 *dir) override;
	oapi::ParticleStream *clbkCreateReentryStream(PARTICLESTREAMSPEC *pss,
		OBJHANDLE hVessel) override;

	// === Screen annotations ===

	oapi::ScreenAnnotation *clbkCreateAnnotation() override;

	// === 2D Drawing ===

	oapi::Font *clbkCreateFont(int height, bool prop, const char *face,
		FontStyle style = FONT_NORMAL, int orientation = 0) const override;
	void clbkReleaseFont(oapi::Font *font) const override;
	oapi::Pen *clbkCreatePen(int style, int width, DWORD col) const override;
	void clbkReleasePen(oapi::Pen *pen) const override;
	oapi::Brush *clbkCreateBrush(DWORD col) const override;
	void clbkReleaseBrush(oapi::Brush *brush) const override;
	oapi::Font *clbkCreateFontEx(int height, char *face, int width = 0, int weight = 400,
		FontStyle style = FONT_NORMAL, float spacing = 0.0f) const override;
	oapi::Sketchpad *clbkGetSketchpad(SURFHANDLE surf) override;
	void clbkReleaseSketchpad(oapi::Sketchpad *sp) override;

	// === Additional surface methods ===

	SURFHANDLE clbkLoadSurface(const char *fname, DWORD attrib, bool bPath = false) override;
	SURFHANDLE clbkCreateTexture(DWORD w, DWORD h) override;
	bool clbkSaveSurfaceToImage(SURFHANDLE surf, const char *fname,
		oapi::ImageFileFormat fmt, float quality = 0.7f) override;

	// === Splash screen ===

	bool clbkSplashLoadMsg(const char *msg, int line) override;
	void clbkSetSplashScreen(const char *fname, DWORD textCol) override;

	// === Mesh persistence ===

	void clbkStoreMeshPersistent(MESHHANDLE hMesh, const char *fname) override;
	int clbkSetMeshMaterialEx(DEVMESHHANDLE hMesh, DWORD matidx, MatProp prp, const oapi::FVECTOR4 *in) override;
	int clbkMeshMaterialEx(DEVMESHHANDLE hMesh, DWORD matidx, MatProp prp, oapi::FVECTOR4 *out) override;

	// === Notification hooks ===

	void clbkOptionChanged(DWORD cat, DWORD item) override;
	void clbkPreOpenPopup() override;
	bool clbkUseLaunchpadVideoTab() const override;

	// === SDL2 integration ===

#ifndef _WIN32
	void SetSDLWindow(SDL_Window *window, SDL_GLContext context);
#endif

private:
#ifndef _WIN32
	SDL_Window *m_sdlWindow;
	SDL_GLContext m_sdlContext;
#endif

	DWORD m_viewW, m_viewH;
	bool m_fullscreen;
	bool m_imguiInitialized;

	// Texture base directory
	std::string m_texturePath;

	// Shader manager and scene renderer
	ShaderMgr *m_shaderMgr;
	OGLScene *m_scene;
	OGLPostProcess *m_postProcess;

	// Blit system (Phase 1)
	GLuint m_blitShader;
	GLuint m_blitVAO, m_blitVBO;
	GLuint m_panel2dShader;
	SURFHANDLE m_bltGroupTgt; // current blt group target (or nullptr)

	void InitBlitResources();
	void ReleaseBlitResources();
	void BlitQuad(OGLSurface *tgt, DWORD tgtx, DWORD tgty, DWORD tgtw, DWORD tgth,
	              OGLSurface *src, DWORD srcx, DWORD srcy, DWORD srcw, DWORD srch,
	              DWORD flag) const;

	// Opt-in round-trip of an RGBA8 render target: create → clear →
	// readback center pixel → log PASS/FAIL. Gated by OGL_M1_SELFTEST=1.
	void RunM1SelfTest();
};

} // namespace ogl

#endif // __OGLCLIENT_H
