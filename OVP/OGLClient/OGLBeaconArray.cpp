// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef _WIN32

#include "OGLBeaconArray.h"
#include "OGLShaderMgr.h"
#include <cstdio>
#include <cmath>

namespace ogl {

GLuint OGLBeaconArray::s_shader = 0;
GLuint OGLBeaconArray::s_vao = 0, OGLBeaconArray::s_vbo = 0;
bool OGLBeaconArray::s_initialized = false;

void OGLBeaconArray::InitShared(ShaderMgr *shaderMgr)
{
	if (s_initialized) return;
	s_initialized = true;
	s_shader = shaderMgr->LoadProgram("beacon", "beacon.vert", "beacon.frag");

	// Billboard quad (updated per-beacon)
	float verts[] = {
		0,0,0, 0,0,  0,0,0, 1,0,
		0,0,0, 0,1,  0,0,0, 1,1,
	};
	glGenVertexArrays(1, &s_vao);
	glGenBuffers(1, &s_vbo);
	glBindVertexArray(s_vao);
	glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
	glBindVertexArray(0);
}

void OGLBeaconArray::ReleaseShared()
{
	if (s_vao) { glDeleteVertexArrays(1, &s_vao); s_vao = 0; }
	if (s_vbo) { glDeleteBuffers(1, &s_vbo); s_vbo = 0; }
	s_initialized = false;
}

OGLBeaconArray::OGLBeaconArray(ShaderMgr *shaderMgr)
	: m_shaderMgr(shaderMgr) {}

OGLBeaconArray::~OGLBeaconArray() {}

void OGLBeaconArray::AddBeacon(const BeaconDef &b)
{
	m_beacons.push_back(b);
}

void OGLBeaconArray::Render(const float *vp, const VECTOR3 &camPos, const MATRIX3 &vesselRot,
                             const VECTOR3 &vesselPos, double simT)
{
	if (!s_shader || !s_vao || m_beacons.empty()) return;

	glUseProgram(s_shader);
	glUniformMatrix4fv(m_shaderMgr->GetUniformLoc(s_shader, "uViewProj"), 1, GL_FALSE, vp);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE); // additive
	glDepthMask(GL_FALSE);

	for (auto &b : m_beacons) {
		// Blink test
		if (b.period > 0) {
			double phase = fmod(simT, (double)b.period) / b.period;
			if (phase > b.duration) continue;
		}

		// Transform beacon position to global
		double gx = vesselRot.m11 * b.pos.x + vesselRot.m12 * b.pos.y + vesselRot.m13 * b.pos.z + vesselPos.x;
		double gy = vesselRot.m21 * b.pos.x + vesselRot.m22 * b.pos.y + vesselRot.m23 * b.pos.z + vesselPos.y;
		double gz = vesselRot.m31 * b.pos.x + vesselRot.m32 * b.pos.y + vesselRot.m33 * b.pos.z + vesselPos.z;

		float px = (float)(gx - camPos.x), py = (float)(gy - camPos.y), pz = (float)(gz - camPos.z);
		float dist = sqrtf(px * px + py * py + pz * pz);
		if (dist > 1e5f) continue;

		// Billboard
		float vdx = px / dist, vdy = py / dist, vdz = pz / dist;
		float ux = 0, uy = 1, uz = 0;
		if (fabsf(vdy) > 0.99f) { ux = 1; uy = 0; }
		float rx = vdy * uz - vdz * uy, ry = vdz * ux - vdx * uz, rz = vdx * uy - vdy * ux;
		float rlen = sqrtf(rx * rx + ry * ry + rz * rz);
		if (rlen > 0) { rx /= rlen; ry /= rlen; rz /= rlen; }
		ux = ry * vdz - rz * vdy; uy = rz * vdx - rx * vdz; uz = rx * vdy - ry * vdx;

		float hw = b.size * 0.5f;
		float verts[] = {
			px - rx * hw - ux * hw, py - ry * hw - uy * hw, pz - rz * hw - uz * hw, 0, 0,
			px + rx * hw - ux * hw, py + ry * hw - uy * hw, pz + rz * hw - uz * hw, 1, 0,
			px - rx * hw + ux * hw, py - ry * hw + uy * hw, pz - rz * hw + uz * hw, 0, 1,
			px + rx * hw + ux * hw, py + ry * hw + uy * hw, pz + rz * hw + uz * hw, 1, 1,
		};

		glBindVertexArray(s_vao);
		glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

		float color[3] = { (float)b.col.x, (float)b.col.y, (float)b.col.z };
		glUniform3fv(m_shaderMgr->GetUniformLoc(s_shader, "uColor"), 1, color);
		glUniform1f(m_shaderMgr->GetUniformLoc(s_shader, "uAlpha"), b.brightness);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	}

	glBindVertexArray(0);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	glUseProgram(0);
}

} // namespace ogl

#endif // !_WIN32
