// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// OpenGL Graphics Client implementation

#ifndef _WIN32

#include "OGLClient.h"
#include "OrbiterAPI.h"
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <vector>

// ImGui SDL2+OpenGL3 backends
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"

namespace ogl {

// ============================================================================
// Shader sources
// ============================================================================

static const char *starVertexShader = R"(
#version 410 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in float aBrightness;
out float vBrightness;
uniform mat4 uViewProj;
void main() {
    gl_Position = uViewProj * vec4(aPos, 1.0);
    gl_PointSize = max(1.0, aBrightness * 3.0);
    vBrightness = aBrightness;
}
)";

static const char *starFragmentShader = R"(
#version 410 core
in float vBrightness;
out vec4 FragColor;
void main() {
    FragColor = vec4(vBrightness, vBrightness, vBrightness * 0.95, 1.0);
}
)";

// ============================================================================
// Helper: compile shader
// ============================================================================

static GLuint CompileShader(GLenum type, const char *source) {
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, nullptr);
	glCompileShader(shader);
	GLint success;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		char log[512];
		glGetShaderInfoLog(shader, 512, nullptr, log);
		fprintf(stderr, "[OGLClient] Shader compile error: %s\n", log);
	}
	return shader;
}

static GLuint CreateShaderProgram(const char *vertSrc, const char *fragSrc) {
	GLuint vert = CompileShader(GL_VERTEX_SHADER, vertSrc);
	GLuint frag = CompileShader(GL_FRAGMENT_SHADER, fragSrc);
	GLuint prog = glCreateProgram();
	glAttachShader(prog, vert);
	glAttachShader(prog, frag);
	glLinkProgram(prog);
	GLint success;
	glGetProgramiv(prog, GL_LINK_STATUS, &success);
	if (!success) {
		char log[512];
		glGetProgramInfoLog(prog, 512, nullptr, log);
		fprintf(stderr, "[OGLClient] Shader link error: %s\n", log);
	}
	glDeleteShader(vert);
	glDeleteShader(frag);
	return prog;
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

OGLClient::OGLClient(HINSTANCE hInstance)
	: GraphicsClient(hInstance),
	  m_sdlWindow(nullptr), m_sdlContext(nullptr),
	  m_viewW(1280), m_viewH(800), m_fullscreen(false),
	  m_imguiInitialized(false),
	  m_starVAO(0), m_starVBO(0), m_starShader(0), m_numStars(0)
{
	fprintf(stderr, "[OGLClient] Created\n");
}

OGLClient::~OGLClient()
{
	fprintf(stderr, "[OGLClient] Destroyed\n");
}

// ============================================================================
// Pure virtual implementations
// ============================================================================

bool OGLClient::clbkFullscreenMode() const
{
	return m_fullscreen;
}

void OGLClient::clbkGetViewportSize(DWORD *width, DWORD *height) const
{
	*width = m_viewW;
	*height = m_viewH;
}

bool OGLClient::clbkGetRenderParam(DWORD prm, DWORD *value) const
{
	switch (prm) {
	case 0x100: *value = 32; return true;  // RP_COLOURDEPTH
	case 0x101: *value = 24; return true;  // RP_ZBUFFERDEPTH
	case 0x102: *value = 8;  return true;  // RP_STENCILDEPTH
	case 0x103: *value = 8;  return true;  // RP_MAXLIGHTS
	default: *value = 0; return false;
	}
}

// ============================================================================
// ImGui integration
// ============================================================================

void OGLClient::clbkImGuiInit()
{
	if (m_imguiInitialized) return;

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	ImGui::StyleColorsDark();

	ImGui_ImplSDL2_InitForOpenGL(m_sdlWindow, m_sdlContext);
	ImGui_ImplOpenGL3_Init("#version 410");

	m_imguiInitialized = true;
	fprintf(stderr, "[OGLClient] ImGui initialized\n");
}

void OGLClient::clbkImGuiShutdown()
{
	if (!m_imguiInitialized) return;

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	m_imguiInitialized = false;
	fprintf(stderr, "[OGLClient] ImGui shutdown\n");
}

void OGLClient::clbkImGuiNewFrame()
{
	if (!m_imguiInitialized) return;
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();
}

void OGLClient::clbkImGuiRenderDrawData()
{
	if (!m_imguiInitialized) return;
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

uint64_t OGLClient::clbkImGuiSurfaceTexture(SURFHANDLE surf)
{
	// TODO: return OpenGL texture ID for the surface
	return 0;
}

// ============================================================================
// Render scene
// ============================================================================

void OGLClient::clbkRenderScene()
{
	glViewport(0, 0, m_viewW, m_viewH);
	glClearColor(0.0f, 0.0f, 0.02f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_PROGRAM_POINT_SIZE);

	// Render starfield
	RenderStarfield();

	// Call base class to render 2D overlay (HUD, panels, ImGui dialogs)
	Render2DOverlay();
}

void OGLClient::RenderStarfield()
{
	if (m_starShader == 0 || m_numStars == 0) return;

	glUseProgram(m_starShader);

	// Simple identity view-projection for now (fixed stars)
	float identity[16] = {
		1,0,0,0,
		0,1,0,0,
		0,0,1,0,
		0,0,0,1
	};
	GLint loc = glGetUniformLocation(m_starShader, "uViewProj");
	glUniformMatrix4fv(loc, 1, GL_FALSE, identity);

	glBindVertexArray(m_starVAO);
	glDrawArrays(GL_POINTS, 0, m_numStars);
	glBindVertexArray(0);
	glUseProgram(0);
}

// ============================================================================
// Session lifecycle
// ============================================================================

HWND OGLClient::clbkCreateRenderWindow()
{
	fprintf(stderr, "[OGLClient] clbkCreateRenderWindow\n");

	// The SDL window is already created by SDLPlatform
	// We just use it here
	if (m_sdlWindow) {
		int w, h;
		SDL_GL_GetDrawableSize(m_sdlWindow, &w, &h);
		m_viewW = w;
		m_viewH = h;
	}

	// Create starfield
	m_numStars = 2000;
	struct StarVertex { float x, y, z, brightness; };
	std::vector<StarVertex> stars(m_numStars);
	srand(42);
	for (int i = 0; i < m_numStars; i++) {
		// Random point on unit sphere
		float theta = ((float)rand() / RAND_MAX) * 2.0f * 3.14159f;
		float phi = acos(2.0f * ((float)rand() / RAND_MAX) - 1.0f);
		float r = 100.0f;
		stars[i].x = r * sin(phi) * cos(theta);
		stars[i].y = r * sin(phi) * sin(theta);
		stars[i].z = r * cos(phi);
		stars[i].brightness = 0.3f + 0.7f * ((float)rand() / RAND_MAX);
	}

	// Create star shader
	m_starShader = CreateShaderProgram(starVertexShader, starFragmentShader);

	// Create star VAO/VBO
	glGenVertexArrays(1, &m_starVAO);
	glGenBuffers(1, &m_starVBO);
	glBindVertexArray(m_starVAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_starVBO);
	glBufferData(GL_ARRAY_BUFFER, stars.size() * sizeof(StarVertex), stars.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(StarVertex), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(StarVertex), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
	glBindVertexArray(0);

	fprintf(stderr, "[OGLClient] Starfield created: %d stars, shader=%u\n", m_numStars, m_starShader);
	return (HWND)m_sdlWindow;
}

void OGLClient::clbkDestroyRenderWindow(bool fastclose)
{
	fprintf(stderr, "[OGLClient] clbkDestroyRenderWindow (fastclose=%d)\n", fastclose);

	if (m_starVAO) { glDeleteVertexArrays(1, &m_starVAO); m_starVAO = 0; }
	if (m_starVBO) { glDeleteBuffers(1, &m_starVBO); m_starVBO = 0; }
	if (m_starShader) { glDeleteProgram(m_starShader); m_starShader = 0; }
}

bool OGLClient::clbkDisplayFrame()
{
	if (m_sdlWindow) {
		SDL_GL_SwapWindow(m_sdlWindow);
		return true;
	}
	return false;
}

void OGLClient::clbkUpdate(bool running)
{
	// Per-frame update before rendering
}

void OGLClient::clbkPostCreation()
{
	fprintf(stderr, "[OGLClient] clbkPostCreation - scenario loaded\n");
}

void OGLClient::clbkCloseSession(bool fastclose)
{
	fprintf(stderr, "[OGLClient] clbkCloseSession\n");
}

// ============================================================================
// Texture/Surface management (minimal stubs)
// ============================================================================

SURFHANDLE OGLClient::clbkLoadTexture(const char *fname, DWORD flags)
{
	// TODO: implement texture loading (DDS, PNG, BMP)
	return nullptr;
}

void OGLClient::clbkReleaseTexture(SURFHANDLE hTex)
{
	// TODO: release OpenGL texture
}

bool OGLClient::clbkReleaseSurface(SURFHANDLE surf)
{
	return true;
}

SURFHANDLE OGLClient::clbkCreateSurfaceEx(DWORD w, DWORD h, DWORD attrib)
{
	// TODO: create OpenGL texture/framebuffer
	return nullptr;
}

bool OGLClient::clbkGetSurfaceSize(SURFHANDLE surf, DWORD *w, DWORD *h)
{
	*w = *h = 0;
	return false;
}

// ============================================================================
// 2D Drawing (minimal stubs)
// ============================================================================

oapi::Font *OGLClient::clbkCreateFont(int height, bool prop, const char *face,
	FontStyle style, int orientation) const
{
	// TODO: implement font rendering
	return nullptr;
}

void OGLClient::clbkReleaseFont(oapi::Font *font) const
{
}

oapi::Sketchpad *OGLClient::clbkGetSketchpad(SURFHANDLE surf)
{
	// TODO: implement 2D drawing
	return nullptr;
}

void OGLClient::clbkReleaseSketchpad(oapi::Sketchpad *sp)
{
}

// ============================================================================
// SDL2 integration
// ============================================================================

void OGLClient::SetSDLWindow(SDL_Window *window, SDL_GLContext context)
{
	m_sdlWindow = window;
	m_sdlContext = context;
}

} // namespace ogl

#endif // !_WIN32
