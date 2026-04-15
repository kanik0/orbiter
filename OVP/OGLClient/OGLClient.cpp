// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// OpenGL Graphics Client implementation

#ifndef _WIN32

#include "OGLClient.h"
#include "OGLTexture.h"
#include "OrbiterAPI.h"
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
#include <sys/stat.h>

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

// Textured planet shaders - uses UV coordinates and texture sampling
static const char *texPlanetVertSrc = R"(
#version 410 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
uniform mat4 uViewProj;
uniform mat4 uModel;
uniform vec3 uSunDir;
out float vLight;
out vec3 vNormal;
out vec2 vUV;
void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    gl_Position = uViewProj * worldPos;
    vec3 worldNormal = normalize(mat3(uModel) * aNormal);
    vLight = max(0.05, dot(worldNormal, uSunDir));
    vNormal = worldNormal;
    vUV = aUV;
}
)";

static const char *texPlanetFragSrc = R"(
#version 410 core
in float vLight;
in vec3 vNormal;
in vec2 vUV;
uniform sampler2D uTexture;
out vec4 FragColor;
void main() {
    vec4 texColor = texture(uTexture, vUV);
    FragColor = vec4(texColor.rgb * vLight, texColor.a);
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

// Build a unit sphere mesh with UV coordinates for texturing
static void CreateTexturedSphere(int slices, int stacks, GLuint &vao, GLuint &vbo, GLuint &ebo, int &indexCount) {
	struct TxVtx { float x, y, z, nx, ny, nz, u, v; };
	std::vector<TxVtx> verts;
	std::vector<unsigned int> indices;

	for (int i = 0; i <= stacks; i++) {
		float phi = M_PI * i / stacks;
		float v = (float)i / stacks;
		for (int j = 0; j <= slices; j++) {
			float theta = 2.0f * M_PI * j / slices;
			float u = (float)j / slices;
			float x = sinf(phi) * cosf(theta);
			float y = cosf(phi);
			float z = sinf(phi) * sinf(theta);
			verts.push_back({x, y, z, x, y, z, u, v});
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
	glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(TxVtx), verts.data(), GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
	// location 0: position
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TxVtx), (void*)0);
	glEnableVertexAttribArray(0);
	// location 1: normal
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(TxVtx), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
	// location 2: UV
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(TxVtx), (void*)(6 * sizeof(float)));
	glEnableVertexAttribArray(2);
	glBindVertexArray(0);
}

// Helper: check if a file exists
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
	  m_starVAO(0), m_starVBO(0), m_starShader(0), m_numStars(0),
	  m_planetShader(0), m_sphereVAO(0), m_sphereVBO(0), m_sphereEBO(0), m_sphereIndexCount(0),
	  m_texPlanetShader(0), m_texSphereVAO(0), m_texSphereVBO(0), m_texSphereEBO(0), m_texSphereIndexCount(0),
	  m_planetTexLoaded(false)
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
	// Disable viewports/platform windows (requires UpdatePlatformWindows which we don't call)
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
	// Only do backend-specific new frame here.
	// DlgMgr::ImGuiNewFrame() calls ImGui::NewFrame() after this.
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame();
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

	// Skip planet rendering if camera state is not yet initialized (NaN)
	bool validCamera = !std::isnan(camPos.x) && !std::isnan(camPos.y) && !std::isnan(camPos.z);

	double fov = oapiCameraAperture() * 2.0; // full vertical FOV
	if (fov <= 0 || std::isnan(fov)) fov = 50.0 * 3.14159 / 180.0;
	double aspect = (double)m_viewW / (double)m_viewH;
	double nearPlane = 1.0;
	double farPlane = 1e10;

	// Projection matrix (OpenGL column-major)
	float f = 1.0f / tanf((float)fov * 0.5f);
	float A = (float)((farPlane + nearPlane) / (nearPlane - farPlane));
	float B = (float)(2.0 * farPlane * nearPlane / (nearPlane - farPlane));
	float proj[16] = {
		f/(float)aspect, 0, 0,  0,
		0,               f, 0,  0,
		0,               0, A, -1,
		0,               0, B,  0
	};

	// View matrix: inverse of camera rotation = transpose (orthonormal)
	// Orbiter's grot: rows are camera-local X,Y,Z axes in global frame
	// Column-major storage: [col0, col1, col2, col3]
	// View = transpose(grot) as column-major
	float view[16] = {
		(float)camRot.m11, (float)camRot.m21, (float)camRot.m31, 0,
		(float)camRot.m12, (float)camRot.m22, (float)camRot.m32, 0,
		(float)camRot.m13, (float)camRot.m23, (float)camRot.m33, 0,
		0,                 0,                  0,                  1
	};

	// View-Projection: P * V (column-major multiply: result[col][row] = sum(P[k][row] * V[col][k]))
	float vp[16];
	for (int col = 0; col < 4; col++)
		for (int row = 0; row < 4; row++) {
			float sum = 0;
			for (int k = 0; k < 4; k++)
				sum += proj[k * 4 + row] * view[col * 4 + k];
			vp[col * 4 + row] = sum;
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

	// 2) Lazy-load planet textures on first frame
	if (!m_planetTexLoaded) {
		m_planetTexLoaded = true;
		DWORD nObj = oapiGetObjectCount();
		for (DWORD i = 0; i < nObj; i++) {
			OBJHANDLE hObj = oapiGetObjectByIndex(i);
			int type = oapiGetObjectType(hObj);
			if (type != OBJTP_PLANET && type != OBJTP_STAR) continue;

			char name[64];
			oapiGetObjectName(hObj, name, 64);
			uintptr_t key = (uintptr_t)hObj;
			OGLTexture *tex = LoadPlanetTexture(name);
			m_planetTexCache[key] = tex; // may be nullptr
		}
	}

	// 3) Render planets (depth test off - we use distance-normalized rendering)
	glDisable(GL_DEPTH_TEST);
	if (m_planetShader && m_sphereVAO && validCamera) {
		// Sun position
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

			// Normalize to prevent float precision issues at astronomical distances.
			// Render each planet at a normalized distance, scaling proportionally.
			double normDist = 10.0; // render at 10 units from camera
			double scale = normDist / dist; // scale factor to normalize distance
			double nrx = rx * scale;
			double nry = ry * scale;
			double nrz = rz * scale;
			float ns = (float)(size * scale);

			float model[16] = {
				ns, 0,  0,  0,
				0,  ns, 0,  0,
				0,  0,  ns, 0,
				(float)nrx, (float)nry, (float)nrz, 1
			};

			// Check if we have a texture for this planet
			uintptr_t key = (uintptr_t)hObj;
			auto it = m_planetTexCache.find(key);
			OGLTexture *planetTex = (it != m_planetTexCache.end()) ? it->second : nullptr;

			if (planetTex && planetTex->texId && m_texPlanetShader && m_texSphereVAO) {
				// Textured rendering path
				glUseProgram(m_texPlanetShader);
				glUniformMatrix4fv(glGetUniformLocation(m_texPlanetShader, "uViewProj"), 1, GL_FALSE, vp);
				glUniformMatrix4fv(glGetUniformLocation(m_texPlanetShader, "uModel"), 1, GL_FALSE, model);
				glUniform3fv(glGetUniformLocation(m_texPlanetShader, "uSunDir"), 1, sunDir);

				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, planetTex->texId);
				glUniform1i(glGetUniformLocation(m_texPlanetShader, "uTexture"), 0);

				glBindVertexArray(m_texSphereVAO);
				glDrawElements(GL_TRIANGLES, m_texSphereIndexCount, GL_UNSIGNED_INT, 0);
			} else {
				// Flat-color fallback
				glUseProgram(m_planetShader);
				glUniformMatrix4fv(glGetUniformLocation(m_planetShader, "uViewProj"), 1, GL_FALSE, vp);
				glUniformMatrix4fv(glGetUniformLocation(m_planetShader, "uModel"), 1, GL_FALSE, model);
				glUniform3fv(glGetUniformLocation(m_planetShader, "uSunDir"), 1, sunDir);

				float color[3];
				if (type == OBJTP_STAR) {
					color[0] = 1.0f; color[1] = 0.95f; color[2] = 0.8f;
				} else {
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

				glBindVertexArray(m_sphereVAO);
				glDrawElements(GL_TRIANGLES, m_sphereIndexCount, GL_UNSIGNED_INT, 0);
			}
		}
		glBindVertexArray(0);
		glBindTexture(GL_TEXTURE_2D, 0);
		glUseProgram(0);
	}

	glEnable(GL_DEPTH_TEST);

	// 4) Render 2D overlay (HUD, panels, ImGui dialogs)
	Render2DOverlay();

	// Debug: save first few frames as BMP for visual inspection
	static int frameCount = 0;
	if (frameCount < 20) {
		frameCount++;
		if (frameCount == 15) { // save 15th frame (let physics stabilize)
			fprintf(stderr, "[OGLClient] Frame %d: cam=(%.3e,%.3e,%.3e) valid=%d\n",
				frameCount, camPos.x, camPos.y, camPos.z, validCamera);
			int w = m_viewW, h = m_viewH;
			std::vector<unsigned char> pixels(w * h * 4);
			glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
			// Write as BMP (bottom-up, RGB)
			FILE *f = fopen("screenshot.bmp", "wb");
			if (f) {
				int rowSize = w * 3;
				int pad = (4 - rowSize % 4) % 4;
				int imgSize = (rowSize + pad) * h;
				// BMP File Header (14 bytes)
				unsigned char bmpFileHeader[14] = {'B','M', 0,0,0,0, 0,0,0,0, 54,0,0,0};
				int fileSize = 54 + imgSize;
				bmpFileHeader[2] = fileSize; bmpFileHeader[3] = fileSize>>8;
				bmpFileHeader[4] = fileSize>>16; bmpFileHeader[5] = fileSize>>24;
				fwrite(bmpFileHeader, 1, 14, f);
				// BMP Info Header (40 bytes)
				unsigned char bmpInfoHeader[40] = {};
				bmpInfoHeader[0] = 40;
				bmpInfoHeader[4] = w; bmpInfoHeader[5] = w>>8;
				bmpInfoHeader[6] = w>>16; bmpInfoHeader[7] = w>>24;
				bmpInfoHeader[8] = h; bmpInfoHeader[9] = h>>8;
				bmpInfoHeader[10] = h>>16; bmpInfoHeader[11] = h>>24;
				bmpInfoHeader[12] = 1; // planes
				bmpInfoHeader[14] = 24; // bpp
				fwrite(bmpInfoHeader, 1, 40, f);
				// Pixel data (OpenGL is bottom-up, BMP is bottom-up, but we need BGR)
				unsigned char padBytes[3] = {0,0,0};
				for (int y = 0; y < h; y++) {
					for (int x = 0; x < w; x++) {
						int idx = (y * w + x) * 4;
						unsigned char bgr[3] = {pixels[idx+2], pixels[idx+1], pixels[idx]};
						fwrite(bgr, 1, 3, f);
					}
					if (pad) fwrite(padBytes, 1, pad, f);
				}
				fclose(f);
				fprintf(stderr, "[OGLClient] Screenshot saved: screenshot.bmp (%dx%d)\n", w, h);
			}
		}
	}
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

	// Create planet shader and sphere mesh (flat color fallback)
	m_planetShader = CreateProgram(planetVertSrc, planetFragSrc);
	CreateSphere(32, 16, m_sphereVAO, m_sphereVBO, m_sphereEBO, m_sphereIndexCount);

	// Create textured planet shader and UV-mapped sphere
	m_texPlanetShader = CreateProgram(texPlanetVertSrc, texPlanetFragSrc);
	CreateTexturedSphere(64, 32, m_texSphereVAO, m_texSphereVBO, m_texSphereEBO, m_texSphereIndexCount);

	// Determine texture search path from the executable location
	// Try to find "Textures/" relative to current working dir or the binary location
	{
		char cwd[1024];
		if (getcwd(cwd, sizeof(cwd))) {
			m_texturePath = std::string(cwd) + "/Textures/";
			struct stat st;
			if (stat(m_texturePath.c_str(), &st) != 0) {
				// Fallback: try parent directory
				m_texturePath = std::string(cwd) + "/../Textures/";
				if (stat(m_texturePath.c_str(), &st) != 0) {
					m_texturePath = "Textures/"; // last resort
				}
			}
		} else {
			m_texturePath = "Textures/";
		}
		fprintf(stderr, "[OGLClient] Texture path: %s\n", m_texturePath.c_str());
	}

	fprintf(stderr, "[OGLClient] Stars: %d, PlanetShader: %u, TexPlanetShader: %u, SphereIdx: %d, TexSphereIdx: %d\n",
		m_numStars, m_planetShader, m_texPlanetShader, m_sphereIndexCount, m_texSphereIndexCount);

	return (HWND)m_sdlWindow;
}

void OGLClient::clbkDestroyRenderWindow(bool fastclose)
{
	fprintf(stderr, "[OGLClient] clbkDestroyRenderWindow\n");

	// Clean up planet texture cache
	for (auto &kv : m_planetTexCache) {
		delete kv.second; // OGLTexture destructor calls glDeleteTextures
	}
	m_planetTexCache.clear();
	m_planetTexLoaded = false;

	if (m_starVAO) { glDeleteVertexArrays(1, &m_starVAO); m_starVAO = 0; }
	if (m_starVBO) { glDeleteBuffers(1, &m_starVBO); m_starVBO = 0; }
	if (m_starShader) { glDeleteProgram(m_starShader); m_starShader = 0; }
	if (m_sphereVAO) { glDeleteVertexArrays(1, &m_sphereVAO); m_sphereVAO = 0; }
	if (m_sphereVBO) { glDeleteBuffers(1, &m_sphereVBO); m_sphereVBO = 0; }
	if (m_sphereEBO) { glDeleteBuffers(1, &m_sphereEBO); m_sphereEBO = 0; }
	if (m_planetShader) { glDeleteProgram(m_planetShader); m_planetShader = 0; }
	if (m_texSphereVAO) { glDeleteVertexArrays(1, &m_texSphereVAO); m_texSphereVAO = 0; }
	if (m_texSphereVBO) { glDeleteBuffers(1, &m_texSphereVBO); m_texSphereVBO = 0; }
	if (m_texSphereEBO) { glDeleteBuffers(1, &m_texSphereEBO); m_texSphereEBO = 0; }
	if (m_texPlanetShader) { glDeleteProgram(m_texPlanetShader); m_texPlanetShader = 0; }
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

SURFHANDLE OGLClient::clbkLoadTexture(const char *fname, DWORD flags)
{
	if (!fname || !fname[0]) return nullptr;

	// Normalize backslash paths from config files
	std::string normalizedName = fname;
	for (auto &c : normalizedName) if (c == '\\') c = '/';

	// Try "Textures/<fname>" first
	std::string tryPath = m_texturePath + normalizedName;
	if (FileExists(tryPath.c_str())) {
		OGLTexture *tex = OGLTexture::LoadTexture(tryPath.c_str());
		if (tex) return (SURFHANDLE)tex;
	}

	// Try raw path
	if (FileExists(normalizedName.c_str())) {
		OGLTexture *tex = OGLTexture::LoadTexture(normalizedName.c_str());
		if (tex) return (SURFHANDLE)tex;
	}

	// Try common extensions if no extension given
	const char *ext = strrchr(normalizedName.c_str(), '.');
	if (!ext) {
		std::string ddsPath = m_texturePath + normalizedName + ".dds";
		if (FileExists(ddsPath.c_str())) {
			OGLTexture *tex = OGLTexture::LoadTexture(ddsPath.c_str());
			if (tex) return (SURFHANDLE)tex;
		}
		std::string bmpPath = m_texturePath + normalizedName + ".bmp";
		if (FileExists(bmpPath.c_str())) {
			OGLTexture *tex = OGLTexture::LoadTexture(bmpPath.c_str());
			if (tex) return (SURFHANDLE)tex;
		}
	}

	fprintf(stderr, "[OGLClient] clbkLoadTexture: not found '%s'\n", fname);
	return nullptr;
}

void OGLClient::clbkReleaseTexture(SURFHANDLE hTex)
{
	if (!hTex) return;
	OGLTexture *tex = (OGLTexture*)hTex;
	delete tex;
}

bool OGLClient::clbkReleaseSurface(SURFHANDLE surf)
{
	if (!surf) return false;
	OGLTexture *tex = (OGLTexture*)surf;
	delete tex;
	return true;
}

SURFHANDLE OGLClient::clbkCreateSurfaceEx(DWORD w, DWORD h, DWORD attrib)
{
	// Create an empty RGBA texture
	GLuint texId = 0;
	glGenTextures(1, &texId);
	glBindTexture(GL_TEXTURE_2D, texId);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);

	OGLTexture *tex = new OGLTexture();
	tex->texId = texId;
	tex->width = w;
	tex->height = h;
	return (SURFHANDLE)tex;
}

bool OGLClient::clbkGetSurfaceSize(SURFHANDLE surf, DWORD *w, DWORD *h)
{
	if (!surf) { *w = *h = 0; return false; }
	OGLTexture *tex = (OGLTexture*)surf;
	*w = tex->width;
	*h = tex->height;
	return true;
}

// ============================================================================
// LoadPlanetTexture - search for a texture file for the named planet
// ============================================================================

OGLTexture *OGLClient::LoadPlanetTexture(const char *planetName)
{
	if (!planetName || !planetName[0]) return nullptr;

	// Try various naming conventions used by Orbiter:
	// Note: <Name>M.bmp are mask/elevation maps, NOT color textures - skip those
	// Real color textures are in .tex container files (not yet supported)
	const char *extensions[] = { ".dds", ".bmp", nullptr };

	for (int i = 0; extensions[i]; i++) {
		std::string tryPath = m_texturePath + planetName + extensions[i];
		if (FileExists(tryPath.c_str())) {
			OGLTexture *tex = OGLTexture::LoadTexture(tryPath.c_str());
			if (tex) {
				fprintf(stderr, "[OGLClient] Loaded planet texture '%s' for %s\n",
					tryPath.c_str(), planetName);
				return tex;
			}
		}
	}

	fprintf(stderr, "[OGLClient] No texture found for planet '%s'\n", planetName);
	return nullptr;
}
oapi::Font *OGLClient::clbkCreateFont(int, bool, const char*, FontStyle, int) const { return nullptr; }
void OGLClient::clbkReleaseFont(oapi::Font*) const {}
oapi::Sketchpad *OGLClient::clbkGetSketchpad(SURFHANDLE) { return nullptr; }
void OGLClient::clbkReleaseSketchpad(oapi::Sketchpad*) {}

void OGLClient::SetSDLWindow(SDL_Window *w, SDL_GLContext c) { m_sdlWindow = w; m_sdlContext = c; }

} // namespace ogl

#endif // !_WIN32
