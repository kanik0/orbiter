// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef _WIN32

#include "OGLCelSphere.h"
#include "OGLShaderMgr.h"
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>

namespace ogl {

OGLCelSphere::OGLCelSphere(ShaderMgr *shaderMgr)
	: m_shaderMgr(shaderMgr), m_vao(0), m_vbo(0), m_shader(0), m_numStars(0),
	  m_coronaVAO(0), m_coronaShader(0),
	  m_gridVAO(0), m_gridVBO(0), m_gridShader(0),
	  m_gridVertCount(0), m_gridEnabled(false)
{
	if (const char *e = std::getenv("OGL_PLANETARIUM"))
		m_gridEnabled = (e[0] == '1');
}

OGLCelSphere::~OGLCelSphere()
{
	Release();
}

void OGLCelSphere::Init(int numStars)
{
	m_numStars = numStars;
	m_shader        = m_shaderMgr->LoadProgram("star",   "star.vert",   "star.frag");
	m_coronaShader  = m_shaderMgr->LoadProgram("corona", "corona.vert", "corona.frag");
	m_gridShader    = m_shaderMgr->LoadProgram("grid",   "grid.vert",   "grid.frag");
	glGenVertexArrays(1, &m_coronaVAO);  // empty VAO — vertices come from gl_VertexID
	BuildGrid();

	struct StarVtx { float x, y, z, brightness, r, g, b; };
	std::vector<StarVtx> stars(numStars);
	srand(42);

	for (int i = 0; i < numStars; i++) {
		float theta = ((float)rand() / RAND_MAX) * 2.0f * M_PI;
		float phi = acosf(2.0f * ((float)rand() / RAND_MAX) - 1.0f);
		float dist = 1e6f;
		stars[i].x = dist * sinf(phi) * cosf(theta);
		stars[i].y = dist * sinf(phi) * sinf(theta);
		stars[i].z = dist * cosf(phi);
		float b = 0.2f + 0.8f * ((float)rand() / RAND_MAX);
		stars[i].brightness = b;
		float temp = 0.8f + 0.4f * ((float)rand() / RAND_MAX);
		stars[i].r = fminf(1.0f, temp);
		stars[i].g = fminf(1.0f, temp * 0.95f);
		stars[i].b = fminf(1.0f, temp * 0.85f + 0.15f);
	}

	glGenVertexArrays(1, &m_vao);
	glGenBuffers(1, &m_vbo);
	glBindVertexArray(m_vao);
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
	glBufferData(GL_ARRAY_BUFFER, stars.size() * sizeof(StarVtx), stars.data(), GL_STATIC_DRAW);
	// location 0: position (vec3)
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(StarVtx), (void*)0);
	glEnableVertexAttribArray(0);
	// location 1: brightness (float)
	glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(StarVtx), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
	// location 2: color (vec3)
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(StarVtx), (void*)(4 * sizeof(float)));
	glEnableVertexAttribArray(2);
	glBindVertexArray(0);
}

void OGLCelSphere::Render(const float *vp, float time,
                           float sunNdcX, float sunNdcY, bool sunVisible,
                           int viewW, int viewH)
{
	if (!m_shader || m_numStars == 0) return;

	// Star pass — additive, so the ACES tonemap downstream can roll off
	// bright stars without punching through the planet surface when an
	// object happens to sit along the same line of sight.
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	glUseProgram(m_shader);
	glUniformMatrix4fv(m_shaderMgr->GetUniformLoc(m_shader, "uViewProj"), 1, GL_FALSE, vp);
	glUniform1f(m_shaderMgr->GetUniformLoc(m_shader, "uTime"), time);
	glBindVertexArray(m_vao);
	glDrawArrays(GL_POINTS, 0, m_numStars);
	glBindVertexArray(0);

	// Planetarium grid — equatorial + ecliptic, toggled by OGL_PLANETARIUM=1.
	if (m_gridEnabled && m_gridShader && m_gridVAO && m_gridVertCount > 0) {
		glUseProgram(m_gridShader);
		glUniformMatrix4fv(m_shaderMgr->GetUniformLoc(m_gridShader, "uViewProj"),
		                   1, GL_FALSE, vp);
		glUniform1f(m_shaderMgr->GetUniformLoc(m_gridShader, "uAlpha"), 0.35f);
		glBindVertexArray(m_gridVAO);
		glBlendFunc(GL_ONE, GL_ONE);
		glDrawArrays(GL_LINES, 0, m_gridVertCount);
		glBindVertexArray(0);
	}

	// Corona pass — painted on a fullscreen triangle strip behind the
	// regular 3D scene (depth=1). `uSunVisible=0` turns the shader into
	// a single `discard` so we don't pay for an unused full-frame quad
	// when the sun is offscreen / behind the camera.
	if (m_coronaShader && m_coronaVAO) {
		glUseProgram(m_coronaShader);
		glBlendFunc(GL_ONE, GL_ONE);   // pure additive for the halo
		glDisable(GL_DEPTH_TEST);
		glBindVertexArray(m_coronaVAO);
		glUniform2f(m_shaderMgr->GetUniformLoc(m_coronaShader, "uSunNdc"),
		            sunNdcX, sunNdcY);
		glUniform2f(m_shaderMgr->GetUniformLoc(m_coronaShader, "uResolution"),
		            (float)std::max(viewW, 1), (float)std::max(viewH, 1));
		glUniform1f(m_shaderMgr->GetUniformLoc(m_coronaShader, "uTime"), time);
		glUniform1f(m_shaderMgr->GetUniformLoc(m_coronaShader, "uIntensity"), 1.0f);
		glUniform1i(m_shaderMgr->GetUniformLoc(m_coronaShader, "uSunVisible"),
		            sunVisible ? 1 : 0);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glBindVertexArray(0);
		glEnable(GL_DEPTH_TEST);
	}

	glDisable(GL_BLEND);
	glUseProgram(0);
}

void OGLCelSphere::Release()
{
	if (m_vao)         { glDeleteVertexArrays(1, &m_vao);        m_vao = 0; }
	if (m_vbo)         { glDeleteBuffers(1, &m_vbo);             m_vbo = 0; }
	if (m_coronaVAO)   { glDeleteVertexArrays(1, &m_coronaVAO);  m_coronaVAO = 0; }
	if (m_gridVAO)     { glDeleteVertexArrays(1, &m_gridVAO);    m_gridVAO = 0; }
	if (m_gridVBO)     { glDeleteBuffers(1, &m_gridVBO);         m_gridVBO = 0; }
	m_gridVertCount = 0;
	m_numStars = 0;
}

// Build the equatorial + ecliptic grid geometry as a pair of line segments
// on a unit celestial sphere, then scale out to the starfield radius so the
// grid shares the star's infinite-distance feel. Equatorial grid uses 12 RA
// meridians (every 30°) and five declination parallels (±60°, ±30°, 0°);
// the ecliptic is a single tilted circle at 23.4388°.
void OGLCelSphere::BuildGrid()
{
	struct GVtx { float x, y, z, r, g, b; };
	std::vector<GVtx> verts;

	const float R         = 1e6f;     // matches the star radius
	const float kEqColor[3]   = { 0.22f, 0.35f, 0.55f };
	const float kEclColor[3]  = { 0.55f, 0.40f, 0.20f };
	const int   kSegmentsArc  = 64;   // circle resolution

	auto emitCircle = [&](float nx, float ny, float nz, const float col[3]) {
		// Build two orthonormal tangents to the normal (nx, ny, nz).
		float ux = 0.0f, uy = 1.0f, uz = 0.0f;
		if (std::fabs(ny) > 0.95f) { ux = 1.0f; uy = 0.0f; uz = 0.0f; }
		float tx = uy * nz - uz * ny;
		float ty = uz * nx - ux * nz;
		float tz = ux * ny - uy * nx;
		float tl = std::sqrt(tx * tx + ty * ty + tz * tz);
		if (tl < 1e-6f) return;
		tx /= tl; ty /= tl; tz /= tl;
		float bx = ny * tz - nz * ty;
		float by = nz * tx - nx * tz;
		float bz = nx * ty - ny * tx;

		for (int i = 0; i < kSegmentsArc; i++) {
			float a0 = 2.0f * float(M_PI) * float(i)       / float(kSegmentsArc);
			float a1 = 2.0f * float(M_PI) * float(i + 1)   / float(kSegmentsArc);
			float c0 = std::cos(a0), s0 = std::sin(a0);
			float c1 = std::cos(a1), s1 = std::sin(a1);
			GVtx p0{ R * (tx * c0 + bx * s0), R * (ty * c0 + by * s0), R * (tz * c0 + bz * s0),
			         col[0], col[1], col[2] };
			GVtx p1{ R * (tx * c1 + bx * s1), R * (ty * c1 + by * s1), R * (tz * c1 + bz * s1),
			         col[0], col[1], col[2] };
			verts.push_back(p0);
			verts.push_back(p1);
		}
	};

	// --- Equatorial RA meridians (every 30°). Each is a great circle whose
	//     normal lies in the equatorial plane (y=0) rotated by the RA.
	for (int k = 0; k < 12; k++) {
		float ra = 2.0f * float(M_PI) * float(k) / 12.0f;
		float nx = std::cos(ra), ny = 0.0f, nz = std::sin(ra);
		// perpendicular to this meridian's plane is actually (-sin, 0, cos),
		// but any normal + orthonormal basis is fine for a full circle.
		emitCircle(nx, ny, nz, kEqColor);
	}

	// --- Declination parallels at ±60°, ±30°, 0° (equator).
	const float kDecs[] = { -60.0f, -30.0f, 0.0f, 30.0f, 60.0f };
	for (float decDeg : kDecs) {
		float dec = decDeg * float(M_PI) / 180.0f;
		float cy  = std::sin(dec);
		float ry  = std::cos(dec);       // radius of the parallel on a unit sphere
		for (int i = 0; i < kSegmentsArc; i++) {
			float a0 = 2.0f * float(M_PI) * float(i)       / float(kSegmentsArc);
			float a1 = 2.0f * float(M_PI) * float(i + 1)   / float(kSegmentsArc);
			GVtx p0{ R * ry * std::cos(a0), R * cy, R * ry * std::sin(a0),
			         kEqColor[0], kEqColor[1], kEqColor[2] };
			GVtx p1{ R * ry * std::cos(a1), R * cy, R * ry * std::sin(a1),
			         kEqColor[0], kEqColor[1], kEqColor[2] };
			verts.push_back(p0);
			verts.push_back(p1);
		}
	}

	// --- Ecliptic: a great circle tilted 23.4388° from the equator.
	{
		const float obliq = 23.4388f * float(M_PI) / 180.0f;
		// Normal to the ecliptic: rotate +Y by `obliq` about X.
		float nx = 0.0f;
		float ny = std::cos(obliq);
		float nz = -std::sin(obliq);
		emitCircle(nx, ny, nz, kEclColor);
	}

	m_gridVertCount = (int)verts.size();
	if (m_gridVertCount == 0) return;

	glGenVertexArrays(1, &m_gridVAO);
	glGenBuffers(1, &m_gridVBO);
	glBindVertexArray(m_gridVAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_gridVBO);
	glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(GVtx), verts.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GVtx), (void *)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GVtx), (void *)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
	glBindVertexArray(0);

	fprintf(stderr, "[OGLCelSphere] planetarium grid built (%d lines, enabled=%d)\n",
	        m_gridVertCount / 2, (int)m_gridEnabled);
}

} // namespace ogl

#endif // !_WIN32
