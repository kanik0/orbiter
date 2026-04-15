// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// OpenGL Graphics Client implementation

#ifndef _WIN32

#include "OGLClient.h"
#include "OGLTexture.h"
#include "OrbiterAPI.h"
#include "VesselAPI.h"
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

// Vessel mesh shader - supports NTVERTEX (pos + normal + UV) with material color
static const char *vesselVertSrc = R"(
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
    vLight = max(0.15, dot(worldNormal, uSunDir));
    vNormal = worldNormal;
    vUV = aUV;
}
)";

static const char *vesselFragSrc = R"(
#version 410 core
in float vLight;
in vec3 vNormal;
in vec2 vUV;
uniform vec4 uDiffuse;
uniform vec3 uEmissive;
uniform bool uHasTexture;
uniform sampler2D uTexture;
out vec4 FragColor;
void main() {
    vec4 baseColor = uDiffuse;
    if (uHasTexture) {
        vec4 texColor = texture(uTexture, vUV);
        baseColor *= texColor;
    }
    vec3 lit = baseColor.rgb * vLight + uEmissive;
    FragColor = vec4(lit, baseColor.a);
}
)";

// Ring shader - renders planetary rings as textured annulus with alpha
// The vertex position is on a unit circle (radius=1); the UV.x (0=outer, 1=inner)
// selects between outer and inner radii via uniforms.
static const char *ringVertSrc = R"(
#version 410 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
uniform mat4 uViewProj;
uniform mat4 uModel;
uniform float uInnerRad;
uniform float uOuterRad;
out vec2 vUV;
void main() {
    float radius = mix(uOuterRad, uInnerRad, aUV.x);
    vec3 scaledPos = aPos * radius;
    vec4 worldPos = uModel * vec4(scaledPos, 1.0);
    gl_Position = uViewProj * worldPos;
    vUV = aUV;
}
)";

static const char *ringFragSrc = R"(
#version 410 core
in vec2 vUV;
uniform sampler2D uTexture;
out vec4 FragColor;
void main() {
    vec4 color = texture(uTexture, vUV);
    float alpha = color.r * 0.75;
    FragColor = vec4(color.rgb, alpha);
}
)";

// Exhaust shader - additive blended billboard quad
static const char *exhaustVertSrc = R"(
#version 410 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
uniform mat4 uViewProj;
out vec2 vUV;
void main() {
    gl_Position = uViewProj * vec4(aPos, 1.0);
    vUV = aUV;
}
)";

static const char *exhaustFragSrc = R"(
#version 410 core
in vec2 vUV;
uniform sampler2D uTexture;
uniform float uAlpha;
out vec4 FragColor;
void main() {
    vec4 color = texture(uTexture, vUV);
    FragColor = vec4(color.rgb * uAlpha, color.a * uAlpha);
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
	  m_planetTexLoaded(false),
	  m_vesselShader(0),
	  m_exhaustShader(0), m_exhaustVAO(0), m_exhaustVBO(0), m_exhaustEBO(0),
	  m_exhaustTexture(nullptr), m_exhaustInitialized(false),
	  m_ringShader(0), m_ringVAO(0), m_ringVBO(0), m_ringEBO(0),
	  m_ringIndexCount(0), m_ringTexture(nullptr), m_ringsInitialized(false)
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

	// Projection matrix (standard OpenGL, camera looks along -Z in view space)
	// NOTE: Orbiter camera looks along +Z. The view matrix produces +Z for
	// objects in front. The -1 in proj[11] combined with the planet distance
	// normalization scheme works for planet rendering but NOT for vessel
	// rendering (vessels end up at +Z in view space). This is a known issue
	// that needs to be resolved with a proper coordinate convention fix.
	float f = 1.0f / tanf((float)fov * 0.5f);
	float A = (float)((farPlane + nearPlane) / (nearPlane - farPlane));
	float B = (float)(2.0 * farPlane * nearPlane / (nearPlane - farPlane));
	float proj[16] = {
		f/(float)aspect, 0, 0,  0,
		0,               f, 0,  0,
		0,               0, A, -1,
		0,               0, B,  0
	};

	// View matrix: transforms from global coordinates to OpenGL eye space.
	//
	// oapiCameraRotationMatrix returns GRot, the camera's local→global
	// rotation (Body.h: "p_glob = GRot * p_loc + GPos"). Its COLUMNS
	// are the camera axes in global frame:
	//   col0 = (m11,m21,m31) = right
	//   col1 = (m12,m22,m32) = up
	//   col2 = (m13,m23,m33) = forward (+Z in Orbiter)
	//
	// The D3D9Client stores GRot directly as the view matrix because D3D
	// uses row-vector multiplication (v*M), which implicitly transposes.
	// OpenGL uses column-vector multiplication (M*v), so we need GRot^T
	// to compute dot(axis, pos) for each camera axis.
	//
	// Additionally, Orbiter's camera looks along +Z, but OpenGL's
	// projection expects the camera to look along -Z. We negate the
	// third row of the transposed matrix to flip the Z axis.
	//
	// Result: V = flipZ * GRot^T   (global → eye with -Z forward)
	//   row0 = GRot col0        →  eye_x = dot(right, pos)
	//   row1 = GRot col1        →  eye_y = dot(up, pos)
	//   row2 = -GRot col2       →  eye_z = -dot(forward, pos)
	float view[16] = {
		(float)camRot.m11,  (float)camRot.m12, -(float)camRot.m13, 0,  // column 0
		(float)camRot.m21,  (float)camRot.m22, -(float)camRot.m23, 0,  // column 1
		(float)camRot.m31,  (float)camRot.m32, -(float)camRot.m33, 0,  // column 2
		0,                  0,                   0,                 1   // column 3
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

	// 3) Render planets (depth test+write off - we use distance-normalized rendering)
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
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

	// 3b) Render planetary rings (still with depth test off for distance-normalized rendering)
	if (validCamera) {
		InitRings(); // lazy init on first use
		VECTOR3 sunPos;
		OBJHANDLE hSun = oapiGetObjectByName((char*)"Sun");
		if (hSun) oapiGetGlobalPos(hSun, &sunPos);
		else sunPos = {0, 0, 0};

		DWORD nObj = oapiGetObjectCount();
		for (DWORD i = 0; i < nObj; i++) {
			OBJHANDLE hObj = oapiGetObjectByIndex(i);
			if (oapiGetObjectType(hObj) == OBJTP_PLANET)
				RenderRings(hObj, camPos, vp, sunPos);
		}
	}

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);

	// 4) Render vessels with mesh data
	if (m_vesselShader && validCamera) {
		// Sun position for lighting
		VECTOR3 sunPos;
		OBJHANDLE hSun = oapiGetObjectByName((char*)"Sun");
		if (hSun) oapiGetGlobalPos(hSun, &sunPos);
		else sunPos = {0, 0, 0};

		glUseProgram(m_vesselShader);
		glUniformMatrix4fv(glGetUniformLocation(m_vesselShader, "uViewProj"), 1, GL_FALSE, vp);

		DWORD nVessel = oapiGetVesselCount();
		static bool vesselDbg = true;
		if (vesselDbg) {
			fprintf(stderr, "[OGLClient] Vessels: %u\n", nVessel);
			vesselDbg = false;
		}
		for (DWORD v = 0; v < nVessel; v++) {
			OBJHANDLE hVessel = oapiGetVesselByIndex(v);
			if (!hVessel) continue;
			VESSEL *vessel = oapiGetVesselInterface(hVessel);
			if (!vessel) continue;

			// Get vessel global position relative to camera
			VECTOR3 vpos;
			oapiGetGlobalPos(hVessel, &vpos);
			double vx = vpos.x - camPos.x;
			double vy = vpos.y - camPos.y;
			double vz = vpos.z - camPos.z;
			double vdist = sqrt(vx*vx + vy*vy + vz*vz);

			static bool vesselDistDbg = true;
			if (vesselDistDbg) {
				char vname[64]; oapiGetObjectName(hVessel, vname, 64);
				fprintf(stderr, "[OGLClient] Vessel '%s': dist=%.1f pos=(%.1f,%.1f,%.1f) meshes=%d\n",
					vname, vdist, vx, vy, vz, vessel->GetMeshCount());
				vesselDistDbg = false;
			}
			// Skip vessels that are too far away (beyond ~100km)
			if (vdist > 1e5) continue;

			// Get vessel rotation matrix
			MATRIX3 vrot;
			oapiGetRotationMatrix(hVessel, &vrot);

			// Sun direction at vessel position (in global frame)
			double sdx = sunPos.x - vpos.x;
			double sdy = sunPos.y - vpos.y;
			double sdz = sunPos.z - vpos.z;
			double sdist = sqrt(sdx*sdx + sdy*sdy + sdz*sdz);
			if (sdist > 0) { sdx /= sdist; sdy /= sdist; sdz /= sdist; }
			float sunDir[3] = {(float)sdx, (float)sdy, (float)sdz};
			glUniform3fv(glGetUniformLocation(m_vesselShader, "uSunDir"), 1, sunDir);

			// Distance normalization to avoid float precision issues.
			// Vessels are small objects at potentially large distances from origin.
			// We normalize the position to keep it within float range.
			double normDist = vdist;
			double scale = 1.0;
			if (vdist > 1000.0) {
				// Normalize: render at a closer distance, scaling the whole scene
				normDist = 1000.0;
				scale = normDist / vdist;
			}
			double nvx = vx * scale;
			double nvy = vy * scale;
			double nvz = vz * scale;

			DWORD nMesh = vessel->GetMeshCount();
			for (DWORD m = 0; m < nMesh; m++) {
				MESHHANDLE hMesh = vessel->GetMeshTemplate(m);
				if (!hMesh) {
					// Fallback: GetMeshTemplate may return null on macOS.
					// Use a static map to cache fallback mesh handles per class
					// so we can render on every frame (not just the first).
					static std::map<std::string, MESHHANDLE> fallbackMeshes;
					const char *className = vessel->GetClassName();
					if (className) {
						auto it = fallbackMeshes.find(className);
						if (it != fallbackMeshes.end()) {
							hMesh = it->second;
						} else {
							hMesh = oapiLoadMeshGlobal(className);
							fallbackMeshes[className] = hMesh;
							if (hMesh)
								fprintf(stderr, "[OGLClient] Loaded fallback mesh '%s': %u groups\n",
									className, oapiMeshGroupCount(hMesh));
						}
					}
					if (!hMesh) continue;
				}

				// Get mesh offset in vessel frame
				VECTOR3 meshOfs = {0, 0, 0};
				vessel->GetMeshOffset(m, meshOfs);

				// Get cached OpenGL buffers for this mesh
				CachedMesh *cached = GetOrCreateMeshCache(hMesh);
				if (!cached) continue;

				// Build model matrix: rotation * translation
				// The model matrix combines vessel rotation with position.
				// Mesh offset is applied in vessel-local space before rotation.
				// Model = Translation(vessel_pos) * Rotation(vrot) * Translation(meshOfs) * Scale(scale)
				// In column-major:

				// First apply mesh offset in local frame, then rotate, then translate
				// offset in world frame = vrot * meshOfs
				double ox = vrot.m11 * meshOfs.x + vrot.m12 * meshOfs.y + vrot.m13 * meshOfs.z;
				double oy = vrot.m21 * meshOfs.x + vrot.m22 * meshOfs.y + vrot.m23 * meshOfs.z;
				double oz = vrot.m31 * meshOfs.x + vrot.m32 * meshOfs.y + vrot.m33 * meshOfs.z;

				float tx = (float)(nvx + ox * scale);
				float ty = (float)(nvy + oy * scale);
				float tz = (float)(nvz + oz * scale);
				float s = (float)scale;

				// Column-major model matrix: Scale * Rotation * Translation
				float model[16] = {
					(float)(vrot.m11 * s), (float)(vrot.m21 * s), (float)(vrot.m31 * s), 0.0f,  // column 0
					(float)(vrot.m12 * s), (float)(vrot.m22 * s), (float)(vrot.m32 * s), 0.0f,  // column 1
					(float)(vrot.m13 * s), (float)(vrot.m23 * s), (float)(vrot.m33 * s), 0.0f,  // column 2
					tx,                    ty,                    tz,                    1.0f   // column 3
				};
				glUniformMatrix4fv(glGetUniformLocation(m_vesselShader, "uModel"), 1, GL_FALSE, model);

				// Get material and texture info for this mesh
				DWORD nMat = oapiMeshMaterialCount(hMesh);
				DWORD nTex = oapiMeshTextureCount(hMesh);

				// Render each group
				for (DWORD g = 0; g < (DWORD)cached->groups.size(); g++) {
					CachedMeshGroup &cmg = cached->groups[g];
					if (!cmg.vao || cmg.indexCount == 0) continue;

					// Get the mesh group info for material/texture indices
					MESHGROUPEX *grp = oapiMeshGroupEx(hMesh, g);

					// Set material uniforms
					float diffuse[4] = {0.8f, 0.8f, 0.8f, 1.0f};
					float emissive[3] = {0.0f, 0.0f, 0.0f};

					if (grp && grp->MtrlIdx > 0 && grp->MtrlIdx <= nMat) {
						MATERIAL *mat = oapiMeshMaterial(hMesh, grp->MtrlIdx - 1);
						if (mat) {
							diffuse[0] = mat->diffuse.r;
							diffuse[1] = mat->diffuse.g;
							diffuse[2] = mat->diffuse.b;
							diffuse[3] = mat->diffuse.a;
							emissive[0] = mat->emissive.r;
							emissive[1] = mat->emissive.g;
							emissive[2] = mat->emissive.b;
						}
					}
					glUniform4fv(glGetUniformLocation(m_vesselShader, "uDiffuse"), 1, diffuse);
					glUniform3fv(glGetUniformLocation(m_vesselShader, "uEmissive"), 1, emissive);

					// Set texture if available
					bool hasTexture = false;
					if (grp && grp->TexIdx > 0 && grp->TexIdx <= nTex) {
						SURFHANDLE hSurf = oapiGetTextureHandle(hMesh, grp->TexIdx);
						if (hSurf) {
							OGLTexture *tex = (OGLTexture*)hSurf;
							if (tex->texId) {
								glActiveTexture(GL_TEXTURE0);
								glBindTexture(GL_TEXTURE_2D, tex->texId);
								glUniform1i(glGetUniformLocation(m_vesselShader, "uTexture"), 0);
								hasTexture = true;
							}
						}
					}
					glUniform1i(glGetUniformLocation(m_vesselShader, "uHasTexture"), hasTexture ? 1 : 0);

					glBindVertexArray(cmg.vao);
					glDrawElements(GL_TRIANGLES, cmg.indexCount, GL_UNSIGNED_INT, 0);
				}
			}
		}
		glBindVertexArray(0);
		glBindTexture(GL_TEXTURE_2D, 0);
		glUseProgram(0);

		// Render exhaust plumes for all vessels
		InitExhaust();
		for (DWORD v = 0; v < nVessel; v++) {
			OBJHANDLE hV = oapiGetVesselByIndex(v);
			if (!hV) continue;
			VESSEL *vsl = oapiGetVesselInterface(hV);
			if (!vsl) continue;
			VECTOR3 vgp; oapiGetGlobalPos(hV, &vgp);
			double vvx = vgp.x - camPos.x, vvy = vgp.y - camPos.y, vvz = vgp.z - camPos.z;
			double vd = sqrt(vvx*vvx + vvy*vvy + vvz*vvz);
			if (vd > 1e5) continue;
			double sc = (vd > 1000.0) ? 1000.0/vd : 1.0;
			MATRIX3 vr; oapiGetRotationMatrix(hV, &vr);
			RenderExhausts(vsl, vr, (float)(vvx*sc), (float)(vvy*sc), (float)(vvz*sc), (float)sc, vp, camPos);
		}
	}

	// 5) Render 2D overlay (HUD, panels, ImGui dialogs)
	Render2DOverlay();
	// Note: screenshot capture moved to clbkDisplayFrame() so it includes
	// ImGui overlay (HUD, dialogs) which is rendered after clbkRenderScene.
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

	// Create vessel mesh shader
	m_vesselShader = CreateProgram(vesselVertSrc, vesselFragSrc);

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

	fprintf(stderr, "[OGLClient] Stars: %d, PlanetShader: %u, TexPlanetShader: %u, VesselShader: %u, SphereIdx: %d, TexSphereIdx: %d\n",
		m_numStars, m_planetShader, m_texPlanetShader, m_vesselShader, m_sphereIndexCount, m_texSphereIndexCount);

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

	// Clean up vessel mesh cache
	for (auto &kv : m_meshCache) {
		delete kv.second; // CachedMesh destructor releases GL objects
	}
	m_meshCache.clear();

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
	if (m_vesselShader) { glDeleteProgram(m_vesselShader); m_vesselShader = 0; }
}

bool OGLClient::clbkDisplayFrame()
{
	// Debug: save screenshot after ALL rendering (3D + ImGui overlay)
	static int frameCount = 0;
	if (frameCount < 20) {
		frameCount++;
		if (frameCount == 15) {
			int w = m_viewW, h = m_viewH;
			std::vector<unsigned char> pixels(w * h * 4);
			glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
			FILE *f = fopen("screenshot.bmp", "wb");
			if (f) {
				int rowSize = w * 3;
				int pad = (4 - rowSize % 4) % 4;
				int imgSize = (rowSize + pad) * h;
				unsigned char bmpFileHeader[14] = {'B','M', 0,0,0,0, 0,0,0,0, 54,0,0,0};
				int fileSize = 54 + imgSize;
				bmpFileHeader[2] = fileSize; bmpFileHeader[3] = fileSize>>8;
				bmpFileHeader[4] = fileSize>>16; bmpFileHeader[5] = fileSize>>24;
				fwrite(bmpFileHeader, 1, 14, f);
				unsigned char bmpInfoHeader[40] = {};
				bmpInfoHeader[0] = 40;
				bmpInfoHeader[4] = w; bmpInfoHeader[5] = w>>8;
				bmpInfoHeader[6] = w>>16; bmpInfoHeader[7] = w>>24;
				bmpInfoHeader[8] = h; bmpInfoHeader[9] = h>>8;
				bmpInfoHeader[10] = h>>16; bmpInfoHeader[11] = h>>24;
				bmpInfoHeader[12] = 1;
				bmpInfoHeader[14] = 24;
				fwrite(bmpInfoHeader, 1, 40, f);
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
		const char *tryExts[] = { ".dds", ".bmp", ".tex", ".png", nullptr };
		for (int i = 0; tryExts[i]; i++) {
			std::string tryP = m_texturePath + normalizedName + tryExts[i];
			if (FileExists(tryP.c_str())) {
				OGLTexture *tex = OGLTexture::LoadTexture(tryP.c_str());
				if (tex) return (SURFHANDLE)tex;
			}
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
// GetOrCreateMeshCache - lazy-upload mesh data to GPU
// ============================================================================

CachedMesh *OGLClient::GetOrCreateMeshCache(MESHHANDLE hMesh)
{
	uintptr_t key = (uintptr_t)hMesh;
	auto it = m_meshCache.find(key);
	if (it != m_meshCache.end()) return it->second;

	// Create new cache entry
	CachedMesh *cached = new CachedMesh();
	DWORD nGrp = oapiMeshGroupCount(hMesh);

	for (DWORD g = 0; g < nGrp; g++) {
		MESHGROUPEX *grp = oapiMeshGroupEx(hMesh, g);
		if (!grp || !grp->nVtx || !grp->nIdx) {
			cached->groups.push_back({0, 0, 0, 0});
			continue;
		}

		CachedMeshGroup cmg;
		cmg.indexCount = (int)grp->nIdx;

		glGenVertexArrays(1, &cmg.vao);
		glGenBuffers(1, &cmg.vbo);
		glGenBuffers(1, &cmg.ebo);

		glBindVertexArray(cmg.vao);

		// Upload NTVERTEX data (32 bytes per vertex: x,y,z,nx,ny,nz,tu,tv)
		glBindBuffer(GL_ARRAY_BUFFER, cmg.vbo);
		glBufferData(GL_ARRAY_BUFFER, grp->nVtx * sizeof(NTVERTEX), grp->Vtx, GL_STATIC_DRAW);

		// Upload WORD indices - convert to unsigned int for OpenGL
		std::vector<unsigned int> indices(grp->nIdx);
		for (DWORD i = 0; i < grp->nIdx; i++)
			indices[i] = (unsigned int)grp->Idx[i];

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cmg.ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, grp->nIdx * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

		// layout(location = 0) in vec3 aPos
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(NTVERTEX), (void*)0);
		glEnableVertexAttribArray(0);
		// layout(location = 1) in vec3 aNormal
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(NTVERTEX), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);
		// layout(location = 2) in vec2 aUV
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(NTVERTEX), (void*)(6 * sizeof(float)));
		glEnableVertexAttribArray(2);

		glBindVertexArray(0);

		cached->groups.push_back(cmg);
	}

	m_meshCache[key] = cached;
	fprintf(stderr, "[OGLClient] Cached mesh %p: %d groups\n", hMesh, (int)nGrp);
	return cached;
}

// ============================================================================
// LoadPlanetTexture - search for a texture file for the named planet
// ============================================================================

OGLTexture *OGLClient::LoadPlanetTexture(const char *planetName)
{
	if (!planetName || !planetName[0]) return nullptr;

	// Try various naming conventions used by Orbiter:
	// .tex = concatenated DDS container (primary planet texture format)
	// .dds = standalone DDS texture
	// .bmp = Windows bitmap fallback
	const char *extensions[] = { ".tex", ".dds", ".bmp", nullptr };

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
// ============================================================================
// Exhaust Rendering
// ============================================================================

void OGLClient::InitExhaust()
{
	if (m_exhaustInitialized) return;
	m_exhaustInitialized = true;

	m_exhaustShader = CreateProgram(exhaustVertSrc, exhaustFragSrc);

	// Load exhaust texture
	std::string path = m_texturePath + "Exhaust.dds";
	if (FileExists(path.c_str()))
		m_exhaustTexture = OGLTexture::LoadDDS(path.c_str());

	// Create a unit quad (vertices updated per-exhaust via glBufferSubData)
	// 4 vertices for the main flame quad
	float verts[] = {
		// pos x,y,z, uv u,v
		0,0,0, 0.24f,0,
		0,0,0, 0.24f,1,
		0,0,0, 0.01f,0,
		0,0,0, 0.01f,1,
	};
	unsigned int idx[] = {0,1,2, 3,2,1};

	glGenVertexArrays(1, &m_exhaustVAO);
	glGenBuffers(1, &m_exhaustVBO);
	glGenBuffers(1, &m_exhaustEBO);
	glBindVertexArray(m_exhaustVAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_exhaustVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_exhaustEBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5*sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5*sizeof(float), (void*)(3*sizeof(float)));
	glEnableVertexAttribArray(1);
	glBindVertexArray(0);
}

void OGLClient::RenderExhausts(VESSEL *vessel, const MATRIX3 &vrot,
                                float tx, float ty, float tz, float scale,
                                const float *vp, const VECTOR3 &camPos)
{
	if (!m_exhaustShader || !m_exhaustVAO || !m_exhaustTexture) return;

	DWORD nExhaust = vessel->GetExhaustCount();
	if (nExhaust == 0) return;

	glUseProgram(m_exhaustShader);
	glUniformMatrix4fv(glGetUniformLocation(m_exhaustShader, "uViewProj"), 1, GL_FALSE, vp);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_exhaustTexture->texId);
	glUniform1i(glGetUniformLocation(m_exhaustShader, "uTexture"), 0);

	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE); // additive blending
	glDepthMask(GL_FALSE);

	// Camera direction for billboarding (from camera-relative vessel position)
	float cdx = -tx, cdy = -ty, cdz = -tz;
	float cdlen = sqrtf(cdx*cdx + cdy*cdy + cdz*cdz);
	if (cdlen > 0) { cdx /= cdlen; cdy /= cdlen; cdz /= cdlen; }

	for (DWORD i = 0; i < nExhaust; i++) {
		double level = vessel->GetExhaustLevel(i);
		if (level < 0.01) continue;

		EXHAUSTSPEC es;
		if (!vessel->GetExhaustSpec(i, &es)) continue;
		if (!es.lpos || !es.ldir) continue;

		float lsz = (float)(es.lsize * level * scale);
		float wsz = (float)(es.wsize * level * scale);
		if (lsz < 0.001f) continue;

		// Exhaust position in vessel-local frame
		VECTOR3 lp = *es.lpos;
		VECTOR3 ld = *es.ldir;
		// Transform to global camera-relative frame
		float ex = (float)(vrot.m11*lp.x + vrot.m12*lp.y + vrot.m13*lp.z) * scale + tx;
		float ey = (float)(vrot.m21*lp.x + vrot.m22*lp.y + vrot.m23*lp.z) * scale + ty;
		float ez = (float)(vrot.m31*lp.x + vrot.m32*lp.y + vrot.m33*lp.z) * scale + tz;
		// Exhaust direction in global frame (negative = flame direction)
		float dx = -(float)(vrot.m11*ld.x + vrot.m12*ld.y + vrot.m13*ld.z);
		float dy = -(float)(vrot.m21*ld.x + vrot.m22*ld.y + vrot.m23*ld.z);
		float dz = -(float)(vrot.m31*ld.x + vrot.m32*ld.y + vrot.m33*ld.z);

		// Billboard: perpendicular to camera-exhaust direction
		float sx = cdy*dz - cdz*dy;
		float sy = cdz*dx - cdx*dz;
		float sz = cdx*dy - cdy*dx;
		float slen = sqrtf(sx*sx + sy*sy + sz*sz);
		if (slen < 1e-6f) { sx = 1; sy = 0; sz = 0; slen = 1; }
		sx /= slen; sy /= slen; sz /= slen;

		// Build quad vertices: base at exhaust pos, tip along direction
		float hw = wsz * 0.5f;
		float verts[] = {
			ex - sx*hw, ey - sy*hw, ez - sz*hw,  0.24f, 0.0f,
			ex + sx*hw, ey + sy*hw, ez + sz*hw,  0.24f, 1.0f,
			ex + dx*lsz - sx*hw*0.2f, ey + dy*lsz - sy*hw*0.2f, ez + dz*lsz - sz*hw*0.2f,  0.01f, 0.0f,
			ex + dx*lsz + sx*hw*0.2f, ey + dy*lsz + sy*hw*0.2f, ez + dz*lsz + sz*hw*0.2f,  0.01f, 1.0f,
		};

		glBindVertexArray(m_exhaustVAO);
		glBindBuffer(GL_ARRAY_BUFFER, m_exhaustVBO);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
		glUniform1f(glGetUniformLocation(m_exhaustShader, "uAlpha"), (float)level);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	}

	glBindVertexArray(0);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	glUseProgram(0);
}

// ============================================================================
// Planetary Ring Rendering
// ============================================================================

void OGLClient::InitRings()
{
	if (m_ringsInitialized) return;
	m_ringsInitialized = true;

	m_ringShader = CreateProgram(ringVertSrc, ringFragSrc);

	// Load ring texture (try high-res first)
	const char *ringTexNames[] = {
		"Saturn_ring_4096.dds", "Saturn_ring_2048.dds", "Saturn_ring_8192.dds", nullptr
	};
	for (int i = 0; ringTexNames[i] && !m_ringTexture; i++) {
		std::string path = m_texturePath + ringTexNames[i];
		if (FileExists(path.c_str()))
			m_ringTexture = OGLTexture::LoadDDS(path.c_str());
	}
	if (!m_ringTexture)
		fprintf(stderr, "[OGLClient] Ring texture not found\n");

	// Generate annulus mesh: inner radius 0 maps to innerRad, outer to outerRad
	// We use unit radii [0.0..1.0] and scale via model matrix
	const int nsect = 72; // angular segments
	const int nverts = (nsect + 1) * 2;
	struct RingVtx { float x, y, z, u, v; };
	std::vector<RingVtx> verts(nverts);
	std::vector<unsigned int> indices;

	for (int i = 0; i <= nsect; i++) {
		float angle = (float)i / nsect * 2.0f * M_PI;
		float ca = cosf(angle), sa = sinf(angle);
		// Outer vertex (u=0)
		verts[i * 2].x = ca;
		verts[i * 2].y = 0;
		verts[i * 2].z = sa;
		verts[i * 2].u = 0.0f;
		verts[i * 2].v = 0.5f;
		// Inner vertex (u=1)
		verts[i * 2 + 1].x = ca;
		verts[i * 2 + 1].y = 0;
		verts[i * 2 + 1].z = sa;
		verts[i * 2 + 1].u = 1.0f;
		verts[i * 2 + 1].v = 0.5f;
	}

	for (int i = 0; i < nsect; i++) {
		int base = i * 2;
		indices.push_back(base);
		indices.push_back(base + 1);
		indices.push_back(base + 2);
		indices.push_back(base + 1);
		indices.push_back(base + 3);
		indices.push_back(base + 2);
	}
	m_ringIndexCount = (int)indices.size();

	glGenVertexArrays(1, &m_ringVAO);
	glGenBuffers(1, &m_ringVBO);
	glGenBuffers(1, &m_ringEBO);
	glBindVertexArray(m_ringVAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_ringVBO);
	glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(RingVtx), verts.data(), GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ringEBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(RingVtx), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(RingVtx), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
	glBindVertexArray(0);
}

void OGLClient::RenderRings(OBJHANDLE hPlanet, const VECTOR3 &camPos,
                            const float *vp, const VECTOR3 &sunPos)
{
	if (!m_ringShader || !m_ringVAO || !m_ringTexture) return;

	bool hasRings = *(bool*)oapiGetObjectParam(hPlanet, OBJPRM_PLANET_HASRINGS);
	if (!hasRings) return;

	double minRad = *(double*)oapiGetObjectParam(hPlanet, OBJPRM_PLANET_RINGMINRAD);
	double maxRad = *(double*)oapiGetObjectParam(hPlanet, OBJPRM_PLANET_RINGMAXRAD);
	double planetSize = oapiGetSize(hPlanet);

	VECTOR3 pos;
	oapiGetGlobalPos(hPlanet, &pos);
	double rx = pos.x - camPos.x;
	double ry = pos.y - camPos.y;
	double rz = pos.z - camPos.z;
	double dist = sqrt(rx*rx + ry*ry + rz*rz);

	// Distance normalization (same scheme as planet rendering)
	double normDist = 10.0;
	double scale = normDist / dist;

	// Planet rotation to orient the ring plane
	MATRIX3 prot;
	oapiGetRotationMatrix(hPlanet, &prot);

	// Build model matrix: translation + planet rotation + ring scale
	// Ring mesh is unit annulus; scale outer vertices by maxRad*planetSize,
	// inner by minRad*planetSize (the vertex positions are at radius 1.0,
	// with u=0 being outer and u=1 being inner — we need two radii)
	// Simpler: render at outer radius, inner vertices at inner/outer ratio
	float outerR = (float)(maxRad * planetSize * scale);
	float innerR = (float)(minRad * planetSize * scale);
	float tx = (float)(rx * scale);
	float ty = (float)(ry * scale);
	float tz = (float)(rz * scale);

	// The ring mesh outer vertices (u=0) are at radius 1.0,
	// inner vertices (u=1) are also at 1.0. We need to scale them differently.
	// Instead, we'll use the shader or rebuild: let's use the model matrix
	// to place the outer ring and store inner/outer ratio for the vertex shader.
	// Actually, simpler: just build the model matrix for outer radius,
	// and in the vertex shader, interpolate position based on UV.
	// But that requires modifying the shader...
	//
	// Simplest approach: set outer vertex positions to outerR, inner to innerR
	// by scaling the unit-circle position. Since we can't change per-vertex
	// in the VBO each frame, let's use a uniform for inner/outer radii.

	// Actually, let me just update the vertex positions. The mesh has outer at
	// radius 1.0 and inner at 1.0 (same!). The u coordinate distinguishes them.
	// I'll modify the vertex shader to scale by radius based on u.
	// For now, use a simpler approach: two uniforms.

	// Model matrix: planet rotation + translation (no scale - shader handles it)
	float s = 1.0f; // scale applied per-vertex in shader
	float model[16] = {
		(float)(prot.m11*s), (float)(prot.m21*s), (float)(prot.m31*s), 0,
		(float)(prot.m12*s), (float)(prot.m22*s), (float)(prot.m32*s), 0,
		(float)(prot.m13*s), (float)(prot.m23*s), (float)(prot.m33*s), 0,
		tx,                  ty,                  tz,                  1
	};

	glUseProgram(m_ringShader);
	glUniformMatrix4fv(glGetUniformLocation(m_ringShader, "uViewProj"), 1, GL_FALSE, vp);
	glUniformMatrix4fv(glGetUniformLocation(m_ringShader, "uModel"), 1, GL_FALSE, model);

	// Sun direction
	double sdx = sunPos.x - pos.x, sdy = sunPos.y - pos.y, sdz = sunPos.z - pos.z;
	double sdist = sqrt(sdx*sdx + sdy*sdy + sdz*sdz);
	if (sdist > 0) { sdx /= sdist; sdy /= sdist; sdz /= sdist; }
	float sunDir[3] = {(float)sdx, (float)sdy, (float)sdz};
	glUniform3fv(glGetUniformLocation(m_ringShader, "uSunDir"), 1, sunDir);

	// Pass inner/outer radii as uniforms
	glUniform1f(glGetUniformLocation(m_ringShader, "uInnerRad"), innerR);
	glUniform1f(glGetUniformLocation(m_ringShader, "uOuterRad"), outerR);
	glUniform1f(glGetUniformLocation(m_ringShader, "uBackFace"), 1.0f);

	// Bind ring texture
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_ringTexture->texId);
	glUniform1i(glGetUniformLocation(m_ringShader, "uTexture"), 0);

	// Enable alpha blending, disable depth write, disable culling
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_CULL_FACE);

	glBindVertexArray(m_ringVAO);
	glDrawElements(GL_TRIANGLES, m_ringIndexCount, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);

	glDisable(GL_BLEND);
	glEnable(GL_CULL_FACE);
	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);
}

// ============================================================================
// OGL Font / Pen / Brush / Sketchpad — ImGui-backed 2D drawing
// ============================================================================

// Helper: convert Orbiter 0xBBGGRR colour to ImGui 0xAABBGGRR (alpha = 0xFF)
static ImU32 OrbiterColToImU32(DWORD col) {
	return IM_COL32(col & 0xFF, (col >> 8) & 0xFF, (col >> 16) & 0xFF, 0xFF);
}

// --- OGLFont ---
class OGLFont : public oapi::Font {
public:
	ImFont *imFont;
	float  fontSize;
	OGLFont(int height, bool prop, const char *face, FontStyle style, int orientation)
		: oapi::Font(height, prop, face, style, orientation),
		  imFont(nullptr), fontSize((float)(height < 0 ? -height : height))
	{
		// Map generic face names to available ImGui fonts.
		// ImGui font atlas is built once at startup; we pick the closest match.
		ImGuiIO &io = ImGui::GetIO();
		if (!io.Fonts || io.Fonts->Fonts.Size == 0) return;
		// Default font (index 0) is Roboto-Medium (proportional)
		// Console font (index 2, if loaded) is Cousine-Regular (monospace)
		if (!prop || (face && (strcmp(face, "Fixed") == 0 || strcmp(face, "Courier") == 0 ||
		              strcmp(face, "Courier New") == 0))) {
			// Use monospace font if available (index 2 from DlgMgr loading order)
			if (io.Fonts->Fonts.Size > 2)
				imFont = io.Fonts->Fonts[2];
			else
				imFont = io.Fonts->Fonts[0];
		} else {
			imFont = io.Fonts->Fonts[0]; // default proportional
		}
	}
};

// --- OGLPen ---
class OGLPen : public oapi::Pen {
public:
	int    style;  // 0=invisible, 1=solid, 2=dashed
	int    width;
	ImU32  color;
	OGLPen(int s, int w, DWORD col) : oapi::Pen(s, w, col), style(s), width(w), color(OrbiterColToImU32(col)) {}
};

// --- OGLBrush ---
class OGLBrush : public oapi::Brush {
public:
	ImU32 color;
	OGLBrush(DWORD col) : oapi::Brush(col), color(OrbiterColToImU32(col)) {}
};

// --- OGLSketchpad ---
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
		oapi::Font *prev = m_font;
		m_font = font;
		return prev;
	}
	oapi::Pen *SetPen(oapi::Pen *pen) override {
		oapi::Pen *prev = m_pen;
		m_pen = pen;
		return prev;
	}
	oapi::Brush *SetBrush(oapi::Brush *brush) override {
		oapi::Brush *prev = m_brush;
		m_brush = brush;
		return prev;
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
		int h = (int)sz;
		int w = (int)(sz * 0.6f); // approximate monospace width
		return MAKELONG(w, h);
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
		float r = (rx + ry) * 0.5f; // ImGui only supports circles; approximate
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
	int m_cx, m_cy;   // current pen position
	int m_ox, m_oy;   // origin offset
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
	// Only create ImGui-backed sketchpad when ImGui is initialized and in a frame
	if (!m_imguiInitialized) return nullptr;
	if (!ImGui::GetCurrentContext()) return nullptr;
	// GetForegroundDrawList requires an active frame; check via GetIO
	if (!ImGui::GetIO().Fonts || !ImGui::GetIO().Fonts->IsBuilt()) return nullptr;
	return new OGLSketchpad(surf, m_viewW, m_viewH);
}

void OGLClient::clbkReleaseSketchpad(oapi::Sketchpad *sp) { delete sp; }

void OGLClient::SetSDLWindow(SDL_Window *w, SDL_GLContext c) { m_sdlWindow = w; m_sdlContext = c; }

} // namespace ogl

#endif // !_WIN32
