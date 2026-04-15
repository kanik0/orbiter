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
#include <map>
#include <string>
#include <vector>
#endif

struct OGLTexture;

// Cached OpenGL buffers for a single mesh group
struct CachedMeshGroup {
	GLuint vao;
	GLuint vbo;
	GLuint ebo;
	int indexCount;
};

// Cached OpenGL buffers for an entire mesh (all groups)
struct CachedMesh {
	std::vector<CachedMeshGroup> groups;
	~CachedMesh() {
		for (auto &g : groups) {
			if (g.vao) glDeleteVertexArrays(1, &g.vao);
			if (g.vbo) glDeleteBuffers(1, &g.vbo);
			if (g.ebo) glDeleteBuffers(1, &g.ebo);
		}
	}
};

namespace ogl {

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

	// === 2D Drawing ===

	oapi::Font *clbkCreateFont(int height, bool prop, const char *face,
		FontStyle style = FONT_NORMAL, int orientation = 0) const override;
	void clbkReleaseFont(oapi::Font *font) const override;
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

	// Starfield
	GLuint m_starVAO, m_starVBO;
	GLuint m_starShader;
	int m_numStars;

	// Planet rendering (flat color fallback)
	GLuint m_planetShader;
	GLuint m_sphereVAO, m_sphereVBO, m_sphereEBO;
	int m_sphereIndexCount;

	// Textured planet rendering
	GLuint m_texPlanetShader;
	GLuint m_texSphereVAO, m_texSphereVBO, m_texSphereEBO;
	int m_texSphereIndexCount;

	// Texture cache: maps OBJHANDLE (as uintptr_t) to planet texture
	std::map<uintptr_t, OGLTexture*> m_planetTexCache;
	bool m_planetTexLoaded; // have we attempted loading textures?

	// Helper: try to find and load a planet texture
	OGLTexture *LoadPlanetTexture(const char *planetName);

	// Texture base directory
	std::string m_texturePath;

	// Vessel rendering
	GLuint m_vesselShader;
	std::map<uintptr_t, CachedMesh*> m_meshCache;

	// Helper: get or create cached OpenGL buffers for a mesh
	CachedMesh *GetOrCreateMeshCache(MESHHANDLE hMesh);
};

} // namespace ogl

#endif // __OGLCLIENT_H
