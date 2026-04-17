// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef _WIN32

#include "OGLCelSphere.h"
#include "OGLShaderMgr.h"
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <vector>

namespace ogl {

OGLCelSphere::OGLCelSphere(ShaderMgr *shaderMgr)
	: m_shaderMgr(shaderMgr), m_vao(0), m_vbo(0), m_shader(0), m_numStars(0),
	  m_coronaVAO(0), m_coronaShader(0)
{
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
	glGenVertexArrays(1, &m_coronaVAO);  // empty VAO — vertices come from gl_VertexID

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
	m_numStars = 0;
}

} // namespace ogl

#endif // !_WIN32
