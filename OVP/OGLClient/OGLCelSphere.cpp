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
	: m_shaderMgr(shaderMgr), m_vao(0), m_vbo(0), m_shader(0), m_numStars(0)
{
}

OGLCelSphere::~OGLCelSphere()
{
	Release();
}

void OGLCelSphere::Init(int numStars)
{
	m_numStars = numStars;
	m_shader = m_shaderMgr->LoadProgram("star", "star.vert", "star.frag");

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

void OGLCelSphere::Render(const float *vp)
{
	if (!m_shader || m_numStars == 0) return;

	glUseProgram(m_shader);
	glUniformMatrix4fv(m_shaderMgr->GetUniformLoc(m_shader, "uViewProj"), 1, GL_FALSE, vp);
	glBindVertexArray(m_vao);
	glDrawArrays(GL_POINTS, 0, m_numStars);
	glBindVertexArray(0);
	glUseProgram(0);
}

void OGLCelSphere::Release()
{
	if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
	if (m_vbo) { glDeleteBuffers(1, &m_vbo); m_vbo = 0; }
	m_numStars = 0;
}

} // namespace ogl

#endif // !_WIN32
