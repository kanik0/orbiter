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
	bool clbkGetSurfaceSize(SURFHANDLE surf, DWORD *w, DWORD *h) override;
	void clbkIncrSurfaceRef(SURFHANDLE surf) override;
	bool clbkSetSurfaceColourKey(SURFHANDLE surf, DWORD ckey) override;
	DWORD clbkGetDeviceColour(BYTE r, BYTE g, BYTE b) override;

	// === 2D Drawing ===

	oapi::Font *clbkCreateFont(int height, bool prop, const char *face,
		FontStyle style = FONT_NORMAL, int orientation = 0) const override;
	void clbkReleaseFont(oapi::Font *font) const override;
	oapi::Pen *clbkCreatePen(int style, int width, DWORD col) const override;
	void clbkReleasePen(oapi::Pen *pen) const override;
	oapi::Brush *clbkCreateBrush(DWORD col) const override;
	void clbkReleaseBrush(oapi::Brush *brush) const override;
	oapi::Sketchpad *clbkGetSketchpad(SURFHANDLE surf) override;
	void clbkReleaseSketchpad(oapi::Sketchpad *sp) override;

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
};

} // namespace ogl

#endif // __OGLCLIENT_H
