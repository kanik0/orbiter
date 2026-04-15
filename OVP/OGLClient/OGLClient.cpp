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

static const char *starVertSrc = R"(
#version 410 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in float aBrightness;
layout(location = 2) in vec3 aColor;
out float vBrightness;
out vec3 vColor;
uniform mat4 uViewProj;
void main() {
    gl_Position = uViewProj * vec4(aPos, 1.0);
    gl_PointSize = max(1.0, aBrightness * 4.0);
    vBrightness = aBrightness;
    vColor = aColor;
}
)";

static const char *starFragSrc = R"(
#version 410 core
in float vBrightness;
in vec3 vColor;
out vec4 FragColor;
void main() {
    FragColor = vec4(vColor * vBrightness, 1.0);
}
)";

static const char *planetVertSrc = R"(
#version 410 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
uniform mat4 uViewProj;
uniform mat4 uModel;
uniform vec3 uSunDir;
out float vLight;
out vec3 vNormal;
void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    gl_Position = uViewProj * worldPos;
    vec3 worldNormal = normalize(mat3(uModel) * aNormal);
    vLight = max(0.05, dot(worldNormal, uSunDir));
    vNormal = worldNormal;
}
)";

static const char *planetFragSrc = R"(
#version 410 core
in float vLight;
in vec3 vNormal;
uniform vec3 uColor;
out vec4 FragColor;
void main() {
    FragColor = vec4(uColor * vLight, 1.0);
}
)";

// ============================================================================
// Helpers
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
		fprintf(stderr, "[OGLClient] Shader error: %s\n", log);
	}
	return shader;
}

static GLuint CreateProgram(const char *vertSrc, const char *fragSrc) {
	GLuint vert = CompileShader(GL_VERTEX_SHADER, vertSrc);
	GLuint frag = CompileShader(GL_FRAGMENT_SHADER, fragSrc);
	GLuint prog = glCreateProgram();
	glAttachShader(prog, vert);
	glAttachShader(prog, frag);
	glLinkProgram(prog);
	GLint ok;
	glGetProgramiv(prog, GL_LINK_STATUS, &ok);
	if (!ok) {
		char log[512];
		glGetProgramInfoLog(prog, 512, nullptr, log);
		fprintf(stderr, "[OGLClient] Link error: %s\n", log);
	}
	glDeleteShader(vert);
	glDeleteShader(frag);
	return prog;
}

// Build a unit sphere mesh
static void CreateSphere(int slices, int stacks, GLuint &vao, GLuint &vbo, GLuint &ebo, int &indexCount) {
	struct Vtx { float x, y, z, nx, ny, nz; };
	std::vector<Vtx> verts;
	std::vector<unsigned int> indices;

	for (int i = 0; i <= stacks; i++) {
		float phi = M_PI * i / stacks;
		for (int j = 0; j <= slices; j++) {
			float theta = 2.0f * M_PI * j / slices;
			float x = sinf(phi) * cosf(theta);
			float y = cosf(phi);
			float z = sinf(phi) * sinf(theta);
			verts.push_back({x, y, z, x, y, z});
		}
	}
	for (int i = 0; i < stacks; i++) {
		for (int j = 0; j < slices; j++) {
			int a = i * (slices + 1) + j;
			int b = a + slices + 1;
			indices.push_back(a); indices.push_back(b); indices.push_back(a + 1);
			indices.push_back(a + 1); indices.push_back(b); indices.push_back(b + 1);
		}
	}
	indexCount = (int)indices.size();

	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);
	glGenBuffers(1, &ebo);
	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vtx), verts.data(), GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
	glBindVertexArray(0);
}

// ============================================================================
// OGLClient
// ============================================================================

OGLClient::OGLClient(HINSTANCE hInstance)
	: GraphicsClient(hInstance),
	  m_sdlWindow(nullptr), m_sdlContext(nullptr),
	  m_viewW(1280), m_viewH(800), m_fullscreen(false),
	  m_imguiInitialized(false),
	  m_starVAO(0), m_starVBO(0), m_starShader(0), m_numStars(0),
	  m_planetShader(0), m_sphereVAO(0), m_sphereVBO(0), m_sphereEBO(0), m_sphereIndexCount(0)
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
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
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
	ImGui::NewFrame();
}

void OGLClient::clbkImGuiRenderDrawData() {
	if (!m_imguiInitialized) return;
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

uint64_t OGLClient::clbkImGuiSurfaceTexture(SURFHANDLE) { return 0; }

// ============================================================================
// Render scene - the main rendering callback
// ============================================================================

void OGLClient::clbkRenderScene()
{
	// Update drawable size (might change on resize or Retina)
	if (m_sdlWindow) {
		int w, h;
		SDL_GL_GetDrawableSize(m_sdlWindow, &w, &h);
		m_viewW = w;
		m_viewH = h;
	}

	glViewport(0, 0, m_viewW, m_viewH);
	glClearColor(0.0f, 0.0f, 0.01f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Build view-projection matrix from Orbiter camera
	VECTOR3 camPos;
	MATRIX3 camRot;
	oapiCameraGlobalPos(&camPos);
	oapiCameraRotationMatrix(&camRot);
	double fov = oapiCameraAperture() * 2.0; // full vertical FOV
	double aspect = (double)m_viewW / (double)m_viewH;
	double nearPlane = 1.0;
	double farPlane = 1e10;

	// Projection matrix (OpenGL style, column-major for glUniformMatrix4fv)
	float f = 1.0f / tanf((float)fov * 0.5f);
	float nf = (float)(nearPlane - farPlane);
	float proj[16] = {
		(float)(f / aspect), 0,    0,                                            0,
		0,                   f,    0,                                            0,
		0,                   0,    (float)((farPlane + nearPlane) / nf),        -1.0f,
		0,                   0,    (float)(2.0 * farPlane * nearPlane / nf),     0
	};

	// View matrix: transpose of camera rotation (camera looks along -Z in view space)
	// Orbiter's grot transforms from camera-local to global
	// For OpenGL view matrix we need the inverse = transpose (it's orthonormal)
	float view[16] = {
		(float)camRot.m11, (float)camRot.m21, (float)camRot.m31, 0,
		(float)camRot.m12, (float)camRot.m22, (float)camRot.m32, 0,
		(float)camRot.m13, (float)camRot.m23, (float)camRot.m33, 0,
		0,                 0,                  0,                  1
	};

	// View-Projection (column-major multiply)
	float vp[16];
	for (int i = 0; i < 4; i++)
		for (int j = 0; j < 4; j++) {
			vp[i * 4 + j] = 0;
			for (int k = 0; k < 4; k++)
				vp[i * 4 + j] += proj[i * 4 + k] * view[k * 4 + j];
		}

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_PROGRAM_POINT_SIZE);

	// 1) Render starfield (at infinite distance - no translation)
	if (m_starShader && m_numStars > 0) {
		glUseProgram(m_starShader);
		glUniformMatrix4fv(glGetUniformLocation(m_starShader, "uViewProj"), 1, GL_FALSE, vp);
		glBindVertexArray(m_starVAO);
		glDrawArrays(GL_POINTS, 0, m_numStars);
		glBindVertexArray(0);
	}

	// 2) Render planets as simple colored spheres
	if (m_planetShader && m_sphereVAO) {
		glUseProgram(m_planetShader);
		glUniformMatrix4fv(glGetUniformLocation(m_planetShader, "uViewProj"), 1, GL_FALSE, vp);

		// Sun direction (from camera pos toward origin, normalized)
		VECTOR3 sunPos;
		OBJHANDLE hSun = oapiGetObjectByName((char*)"Sun");
		if (hSun) oapiGetGlobalPos(hSun, &sunPos);
		else sunPos = {0, 0, 0};

		// Enumerate objects and render planets
		DWORD nObj = oapiGetObjectCount();
		for (DWORD i = 0; i < nObj; i++) {
			OBJHANDLE hObj = oapiGetObjectByIndex(i);
			int type = oapiGetObjectType(hObj);
			if (type != OBJTP_PLANET && type != OBJTP_STAR) continue;

			VECTOR3 pos;
			oapiGetGlobalPos(hObj, &pos);

			// Position relative to camera
			double rx = pos.x - camPos.x;
			double ry = pos.y - camPos.y;
			double rz = pos.z - camPos.z;
			double dist = sqrt(rx*rx + ry*ry + rz*rz);

			double size = oapiGetSize(hObj);
			if (dist < size * 0.5) continue; // inside planet

			// Sun direction at this object's position
			double sdx = sunPos.x - pos.x;
			double sdy = sunPos.y - pos.y;
			double sdz = sunPos.z - pos.z;
			double sdist = sqrt(sdx*sdx + sdy*sdy + sdz*sdz);
			if (sdist > 0) { sdx /= sdist; sdy /= sdist; sdz /= sdist; }
			float sunDir[3] = {(float)sdx, (float)sdy, (float)sdz};
			glUniform3fv(glGetUniformLocation(m_planetShader, "uSunDir"), 1, sunDir);

			// Color based on object type
			float color[3];
			if (type == OBJTP_STAR) {
				color[0] = 1.0f; color[1] = 0.95f; color[2] = 0.8f;
			} else {
				// Determine planet color by name
				char name[64];
				oapiGetObjectName(hObj, name, 64);
				if (strcmp(name, "Earth") == 0) { color[0] = 0.2f; color[1] = 0.4f; color[2] = 0.8f; }
				else if (strcmp(name, "Moon") == 0) { color[0] = 0.7f; color[1] = 0.7f; color[2] = 0.7f; }
				else if (strcmp(name, "Mars") == 0) { color[0] = 0.8f; color[1] = 0.3f; color[2] = 0.1f; }
				else if (strcmp(name, "Venus") == 0) { color[0] = 0.9f; color[1] = 0.8f; color[2] = 0.6f; }
				else if (strcmp(name, "Jupiter") == 0) { color[0] = 0.8f; color[1] = 0.7f; color[2] = 0.5f; }
				else if (strcmp(name, "Saturn") == 0) { color[0] = 0.9f; color[1] = 0.8f; color[2] = 0.6f; }
				else { color[0] = 0.6f; color[1] = 0.6f; color[2] = 0.6f; }
			}
			glUniform3fv(glGetUniformLocation(m_planetShader, "uColor"), 1, color);

			// Model matrix: translate + scale
			float s = (float)size;
			float model[16] = {
				s, 0, 0, 0,
				0, s, 0, 0,
				0, 0, s, 0,
				(float)rx, (float)ry, (float)rz, 1
			};
			glUniformMatrix4fv(glGetUniformLocation(m_planetShader, "uModel"), 1, GL_FALSE, model);

			glBindVertexArray(m_sphereVAO);
			glDrawElements(GL_TRIANGLES, m_sphereIndexCount, GL_UNSIGNED_INT, 0);
		}
		glBindVertexArray(0);
		glUseProgram(0);
	}

	// 3) Render 2D overlay (HUD, panels, ImGui dialogs)
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

	// Create starfield
	m_numStars = 4000;
	struct SV { float x, y, z, brightness, r, g, b; };
	std::vector<SV> stars(m_numStars);
	srand(42);
	for (int i = 0; i < m_numStars; i++) {
		float theta = ((float)rand() / RAND_MAX) * 2.0f * M_PI;
		float phi = acosf(2.0f * ((float)rand() / RAND_MAX) - 1.0f);
		float dist = 1e6f; // very far away
		stars[i].x = dist * sinf(phi) * cosf(theta);
		stars[i].y = dist * sinf(phi) * sinf(theta);
		stars[i].z = dist * cosf(phi);
		float b = 0.2f + 0.8f * ((float)rand() / RAND_MAX);
		stars[i].brightness = b;
		// Slight color variation
		float temp = 0.8f + 0.4f * ((float)rand() / RAND_MAX);
		stars[i].r = fminf(1.0f, temp);
		stars[i].g = fminf(1.0f, temp * 0.95f);
		stars[i].b = fminf(1.0f, temp * 0.85f + 0.15f);
	}

	m_starShader = CreateProgram(starVertSrc, starFragSrc);
	glGenVertexArrays(1, &m_starVAO);
	glGenBuffers(1, &m_starVBO);
	glBindVertexArray(m_starVAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_starVBO);
	glBufferData(GL_ARRAY_BUFFER, stars.size() * sizeof(SV), stars.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SV), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(SV), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(SV), (void*)(4 * sizeof(float)));
	glEnableVertexAttribArray(2);
	glBindVertexArray(0);

	// Create planet shader and sphere mesh
	m_planetShader = CreateProgram(planetVertSrc, planetFragSrc);
	CreateSphere(32, 16, m_sphereVAO, m_sphereVBO, m_sphereEBO, m_sphereIndexCount);

	fprintf(stderr, "[OGLClient] Stars: %d, PlanetShader: %u, SphereIdx: %d\n",
		m_numStars, m_planetShader, m_sphereIndexCount);

	return (HWND)m_sdlWindow;
}

void OGLClient::clbkDestroyRenderWindow(bool fastclose)
{
	fprintf(stderr, "[OGLClient] clbkDestroyRenderWindow\n");
	if (m_starVAO) { glDeleteVertexArrays(1, &m_starVAO); m_starVAO = 0; }
	if (m_starVBO) { glDeleteBuffers(1, &m_starVBO); m_starVBO = 0; }
	if (m_starShader) { glDeleteProgram(m_starShader); m_starShader = 0; }
	if (m_sphereVAO) { glDeleteVertexArrays(1, &m_sphereVAO); m_sphereVAO = 0; }
	if (m_sphereVBO) { glDeleteBuffers(1, &m_sphereVBO); m_sphereVBO = 0; }
	if (m_sphereEBO) { glDeleteBuffers(1, &m_sphereEBO); m_sphereEBO = 0; }
	if (m_planetShader) { glDeleteProgram(m_planetShader); m_planetShader = 0; }
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
// Stubs (to be expanded)
// ============================================================================

SURFHANDLE OGLClient::clbkLoadTexture(const char*, DWORD) { return nullptr; }
void OGLClient::clbkReleaseTexture(SURFHANDLE) {}
bool OGLClient::clbkReleaseSurface(SURFHANDLE) { return true; }
SURFHANDLE OGLClient::clbkCreateSurfaceEx(DWORD, DWORD, DWORD) { return nullptr; }
bool OGLClient::clbkGetSurfaceSize(SURFHANDLE, DWORD *w, DWORD *h) { *w = *h = 0; return false; }
oapi::Font *OGLClient::clbkCreateFont(int, bool, const char*, FontStyle, int) const { return nullptr; }
void OGLClient::clbkReleaseFont(oapi::Font*) const {}
oapi::Sketchpad *OGLClient::clbkGetSketchpad(SURFHANDLE) { return nullptr; }
void OGLClient::clbkReleaseSketchpad(oapi::Sketchpad*) {}

void OGLClient::SetSDLWindow(SDL_Window *w, SDL_GLContext c) { m_sdlWindow = w; m_sdlContext = c; }

} // namespace ogl

#endif // !_WIN32
