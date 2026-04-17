// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef _WIN32

#include "OGLAtmosphere.h"
#include "OGLShaderMgr.h"
#include <cstdio>
#include <cmath>
#include <vector>

namespace ogl {

static const int HAZE_NSEG = 64; // azimuthal segments

OGLAtmosphere::OGLAtmosphere(OBJHANDLE hPlanet, ShaderMgr *shaderMgr)
	: m_hPlanet(hPlanet), m_shaderMgr(shaderMgr), m_hasAtmo(false),
	  m_atmoAlt(0), m_horizonAlt(0), m_skyColor{0,0,0}, m_hazeDensity(0), m_hazeExtent(0),
	  m_hazeVAO(0), m_hazeVBO(0), m_hazeEBO(0), m_hazeIndexCount(0), m_hazeShader(0)
{
	m_hasAtmo = oapiPlanetHasAtmosphere(hPlanet);
	if (!m_hasAtmo) return;

	const ATMCONST *atm = oapiGetPlanetAtmConstants(hPlanet);
	if (atm) {
		m_atmoAlt = atm->altlimit;
		m_horizonAlt = atm->horizonalt;
		m_skyColor = atm->color0;
	}

	// Additional haze parameters from object params
	const void *p;
	p = oapiGetObjectParam(hPlanet, OBJPRM_PLANET_HAZEDENSITY);
	m_hazeDensity = p ? *(const double*)p : 1.0;
	p = oapiGetObjectParam(hPlanet, OBJPRM_PLANET_HAZEEXTENT);
	m_hazeExtent = p ? *(const double*)p : 0.1;

	char name[64];
	oapiGetObjectName(hPlanet, name, 64);
	fprintf(stderr, "[OGLAtmosphere] %s: atmoAlt=%.0f horizonAlt=%.0f sky=(%.2f,%.2f,%.2f) haze=%.2f\n",
		name, m_atmoAlt, m_horizonAlt, m_skyColor.x, m_skyColor.y, m_skyColor.z, m_hazeDensity);

	InitHazeRing();
}

OGLAtmosphere::~OGLAtmosphere()
{
	ReleaseHazeRing();
}

void OGLAtmosphere::InitHazeRing()
{
	m_hazeShader = m_shaderMgr->LoadProgram("haze", "haze.vert", "haze.frag");

	// Build haze ring: 2 concentric rings of vertices
	// Inner ring at horizon, outer ring slightly above
	struct HVtx { float x, y, z, alpha; };
	std::vector<HVtx> verts;
	std::vector<unsigned int> indices;

	for (int i = 0; i <= HAZE_NSEG; i++) {
		float angle = (float)i / HAZE_NSEG * 2.0f * M_PI;
		float ca = cosf(angle), sa = sinf(angle);

		// Outer vertex (top of haze, alpha = 0)
		verts.push_back({ca, 0.02f, sa, 0.0f});
		// Inner vertex (at horizon, alpha = 1)
		verts.push_back({ca, 0.0f, sa, 1.0f});
	}

	for (int i = 0; i < HAZE_NSEG; i++) {
		int base = i * 2;
		indices.push_back(base);
		indices.push_back(base + 1);
		indices.push_back(base + 2);
		indices.push_back(base + 1);
		indices.push_back(base + 3);
		indices.push_back(base + 2);
	}
	m_hazeIndexCount = (int)indices.size();

	glGenVertexArrays(1, &m_hazeVAO);
	glGenBuffers(1, &m_hazeVBO);
	glGenBuffers(1, &m_hazeEBO);
	glBindVertexArray(m_hazeVAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_hazeVBO);
	glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(HVtx), verts.data(), GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_hazeEBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
	// location 0: position (vec3)
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(HVtx), (void*)0);
	glEnableVertexAttribArray(0);
	// location 1: alpha (float)
	glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(HVtx), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
	glBindVertexArray(0);
}

void OGLAtmosphere::ReleaseHazeRing()
{
	if (m_hazeVAO) { glDeleteVertexArrays(1, &m_hazeVAO); m_hazeVAO = 0; }
	if (m_hazeVBO) { glDeleteBuffers(1, &m_hazeVBO); m_hazeVBO = 0; }
	if (m_hazeEBO) { glDeleteBuffers(1, &m_hazeEBO); m_hazeEBO = 0; }
}

void OGLAtmosphere::Render(const float *vp, const VECTOR3 &camPos, const VECTOR3 &sunPos,
                            double planetRadius, const VECTOR3 &planetPos)
{
	if (!m_hasAtmo || !m_hazeShader || !m_hazeVAO) return;

	double rx = planetPos.x - camPos.x;
	double ry = planetPos.y - camPos.y;
	double rz = planetPos.z - camPos.z;
	double dist = sqrt(rx * rx + ry * ry + rz * rz);
	double camAlt = dist - planetRadius;

	// Only render haze when camera is within ~5x atmosphere altitude
	if (camAlt > m_atmoAlt * 5.0 || camAlt < 0) return;

	// Haze opacity depends on distance and altitude
	double altFactor = 1.0 - camAlt / (m_atmoAlt * 3.0);
	if (altFactor < 0) altFactor = 0;
	float hazeAlpha = (float)(m_hazeDensity * altFactor);
	if (hazeAlpha < 0.01f) return;

	// Distance normalization (same scheme as planet rendering)
	double normDist = 10.0;
	double scale = normDist / dist;

	// Haze ring positioned at planet horizon
	double hazeRadius = planetRadius + m_horizonAlt;
	float ns = (float)(hazeRadius * scale);
	float tx = (float)(rx * scale), ty = (float)(ry * scale), tz = (float)(rz * scale);

	// Model matrix: scale the ring to planet size, centered at planet pos
	float model[16] = {
		ns, 0,  0,  0,
		0,  ns, 0,  0,
		0,  0,  ns, 0,
		tx, ty, tz, 1
	};

	// Sun direction relative to planet for sky coloring
	double sdx = sunPos.x - planetPos.x, sdy = sunPos.y - planetPos.y, sdz = sunPos.z - planetPos.z;
	double sdist = sqrt(sdx * sdx + sdy * sdy + sdz * sdz);
	if (sdist > 0) { sdx /= sdist; sdy /= sdist; sdz /= sdist; }

	// Camera direction from planet center
	double cdx = -rx / dist, cdy = -ry / dist, cdz = -rz / dist;
	// Sun angle at camera position (for sunset/sunrise coloring)
	double sunAngle = sdx * cdx + sdy * cdy + sdz * cdz;

	// Sky color modulated by sun angle
	float skyR = (float)m_skyColor.x, skyG = (float)m_skyColor.y, skyB = (float)m_skyColor.z;
	if (sunAngle < 0) {
		// Night side: darken
		float nightFactor = (float)(1.0 + sunAngle * 3.0);
		if (nightFactor < 0.05f) nightFactor = 0.05f;
		skyR *= nightFactor; skyG *= nightFactor; skyB *= nightFactor;
	} else if (sunAngle < 0.15) {
		// Sunset/sunrise: warm colors
		float t = (float)(sunAngle / 0.15);
		skyR = skyR * t + 0.9f * (1 - t);
		skyG = skyG * t + 0.4f * (1 - t);
		skyB = skyB * t + 0.2f * (1 - t);
	}

	glUseProgram(m_hazeShader);
	glUniformMatrix4fv(m_shaderMgr->GetUniformLoc(m_hazeShader, "uViewProj"), 1, GL_FALSE, vp);
	glUniformMatrix4fv(m_shaderMgr->GetUniformLoc(m_hazeShader, "uModel"), 1, GL_FALSE, model);
	glUniform3f(m_shaderMgr->GetUniformLoc(m_hazeShader, "uHazeColor"), skyR, skyG, skyB);
	glUniform1f(m_shaderMgr->GetUniformLoc(m_hazeShader, "uHazeAlpha"), hazeAlpha);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	glBindVertexArray(m_hazeVAO);
	glDrawElements(GL_TRIANGLES, m_hazeIndexCount, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glUseProgram(0);
}

} // namespace ogl

#endif // !_WIN32
