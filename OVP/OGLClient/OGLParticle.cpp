// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef _WIN32

#include "OGLParticle.h"
#include "OGLShaderMgr.h"
#include "OGLTexture.h"
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <sys/stat.h>

namespace ogl {

GLuint OGLParticleStream::s_shader = 0;
GLuint OGLParticleStream::s_vao = 0, OGLParticleStream::s_vbo = 0;
OGLTexture *OGLParticleStream::s_defaultTex = nullptr;
bool OGLParticleStream::s_initialized = false;
ShaderMgr *OGLParticleStream::s_shaderMgr = nullptr;

static bool FileExists(const char *p) { struct stat st; return stat(p, &st) == 0; }

void OGLParticleStream::InitShared(ShaderMgr *shaderMgr, const std::string &texturePath)
{
	if (s_initialized) return;
	s_initialized = true;
	s_shaderMgr = shaderMgr;
	s_shader = shaderMgr->LoadProgram("particle", "particle.vert", "particle.frag");

	// Load default particle texture
	std::string path = texturePath + "Particle.dds";
	if (FileExists(path.c_str()))
		s_defaultTex = OGLTexture::LoadDDS(path.c_str());
	if (!s_defaultTex) {
		path = texturePath + "Exhaust.dds";
		if (FileExists(path.c_str()))
			s_defaultTex = OGLTexture::LoadDDS(path.c_str());
	}

	// Quad billboard (4 verts: pos.xyz + uv.xy = 5 floats)
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

void OGLParticleStream::ReleaseShared()
{
	if (s_vao) { glDeleteVertexArrays(1, &s_vao); s_vao = 0; }
	if (s_vbo) { glDeleteBuffers(1, &s_vbo); s_vbo = 0; }
	delete s_defaultTex; s_defaultTex = nullptr;
	s_initialized = false;
}

OGLParticleStream::OGLParticleStream(oapi::GraphicsClient *gc, PARTICLESTREAMSPEC *pss, ShaderMgr *shaderMgr)
	: oapi::ParticleStream(gc, pss), m_shaderMgr(shaderMgr), m_lastEmit(0), m_active(true)
{
	if (pss) m_spec = *pss;
	else memset(&m_spec, 0, sizeof(m_spec));
}

OGLParticleStream::~OGLParticleStream() {}

void OGLParticleStream::EmitParticle(double simT)
{
	if (!level || *level < 0.01) return;

	Particle p;
	// Get source position in global frame
	if (hRef && pos) {
		VECTOR3 gpos, rp;
		oapiGetGlobalPos(hRef, &gpos);
		MATRIX3 rot;
		oapiGetRotationMatrix(hRef, &rot);
		// Transform local pos to global
		rp.x = rot.m11 * pos->x + rot.m12 * pos->y + rot.m13 * pos->z;
		rp.y = rot.m21 * pos->x + rot.m22 * pos->y + rot.m23 * pos->z;
		rp.z = rot.m31 * pos->x + rot.m32 * pos->y + rot.m33 * pos->z;
		p.pos = { gpos.x + rp.x, gpos.y + rp.y, gpos.z + rp.z };

		// Velocity: vessel velocity + exhaust direction
		VECTOR3 vvel;
		oapiGetGlobalVel(hRef, &vvel);
		VECTOR3 gdir = {0, 0, 0};
		if (dir) {
			gdir.x = rot.m11 * dir->x + rot.m12 * dir->y + rot.m13 * dir->z;
			gdir.y = rot.m21 * dir->x + rot.m22 * dir->y + rot.m23 * dir->z;
			gdir.z = rot.m31 * dir->x + rot.m32 * dir->y + rot.m33 * dir->z;
		}
		double v0 = m_spec.v0 * (*level);
		p.vel = { vvel.x + gdir.x * v0, vvel.y + gdir.y * v0, vvel.z + gdir.z * v0 };
	} else {
		p.pos = lpos;
		double v0 = m_spec.v0;
		p.vel = { ldir.x * v0, ldir.y * v0, ldir.z * v0 };
	}

	p.size = (float)(m_spec.srcsize * (*level));
	p.alpha = 1.0f;
	p.age = 0;
	p.lifetime = (float)m_spec.lifetime;
	if (p.lifetime <= 0) p.lifetime = 2.0f;

	// Add random spread
	float spread = (float)m_spec.srcspread;
	if (spread > 0) {
		p.vel.x += (((float)rand() / RAND_MAX) - 0.5f) * spread * m_spec.v0;
		p.vel.y += (((float)rand() / RAND_MAX) - 0.5f) * spread * m_spec.v0;
		p.vel.z += (((float)rand() / RAND_MAX) - 0.5f) * spread * m_spec.v0;
	}

	m_particles.push_back(p);
}

void OGLParticleStream::Update(double simT, double dt)
{
	if (!m_active) return;

	// Emit new particles
	double emitInterval = (m_spec.srcrate > 0) ? 1.0 / m_spec.srcrate : 0.05;
	if (simT - m_lastEmit >= emitInterval && level && *level > 0.01) {
		EmitParticle(simT);
		m_lastEmit = simT;
	}

	// Update existing particles
	for (auto it = m_particles.begin(); it != m_particles.end(); ) {
		it->age += (float)dt;
		if (it->age >= it->lifetime) {
			it = m_particles.erase(it);
			continue;
		}

		// Move
		it->pos.x += it->vel.x * dt;
		it->pos.y += it->vel.y * dt;
		it->pos.z += it->vel.z * dt;

		// Grow
		it->size += (float)(m_spec.growthrate * dt);

		// Fade
		float t = it->age / it->lifetime;
		it->alpha = 1.0f - t;

		// Atmospheric slowdown
		if (m_spec.atmslowdown > 0) {
			double drag = 1.0 - m_spec.atmslowdown * dt;
			if (drag < 0.5) drag = 0.5;
			it->vel.x *= drag;
			it->vel.y *= drag;
			it->vel.z *= drag;
		}

		++it;
	}
}

void OGLParticleStream::Render(const float *vp, const VECTOR3 &camPos)
{
	if (!s_shader || !s_vao || m_particles.empty()) return;

	glUseProgram(s_shader);
	glUniformMatrix4fv(s_shaderMgr->GetUniformLoc(s_shader, "uViewProj"), 1, GL_FALSE, vp);

	if (s_defaultTex) {
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, s_defaultTex->texId);
		glUniform1i(s_shaderMgr->GetUniformLoc(s_shader, "uTexture"), 0);
	}

	bool isEmissive = (m_spec.ltype == PARTICLESTREAMSPEC::EMISSIVE);
	glEnable(GL_BLEND);
	if (isEmissive)
		glBlendFunc(GL_ONE, GL_ONE); // additive
	else
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_FALSE);

	// Camera direction for billboarding
	for (auto &p : m_particles) {
		float px = (float)(p.pos.x - camPos.x);
		float py = (float)(p.pos.y - camPos.y);
		float pz = (float)(p.pos.z - camPos.z);

		float dist = sqrtf(px * px + py * py + pz * pz);
		if (dist < 0.01f || dist > 1e5f) continue;

		// Billboard vectors (perpendicular to view direction)
		float vdx = px / dist, vdy = py / dist, vdz = pz / dist;
		// Choose up vector that isn't parallel to view
		float ux = 0, uy = 1, uz = 0;
		if (fabsf(vdy) > 0.99f) { ux = 1; uy = 0; }
		// right = cross(view, up)
		float rx = vdy * uz - vdz * uy;
		float ry = vdz * ux - vdx * uz;
		float rz = vdx * uy - vdy * ux;
		float rlen = sqrtf(rx * rx + ry * ry + rz * rz);
		if (rlen > 0) { rx /= rlen; ry /= rlen; rz /= rlen; }
		// recompute up = cross(right, view)
		ux = ry * vdz - rz * vdy;
		uy = rz * vdx - rx * vdz;
		uz = rx * vdy - ry * vdx;

		float hw = p.size * 0.5f;

		float verts[] = {
			px - rx * hw - ux * hw, py - ry * hw - uy * hw, pz - rz * hw - uz * hw, 0, 0,
			px + rx * hw - ux * hw, py + ry * hw - uy * hw, pz + rz * hw - uz * hw, 1, 0,
			px - rx * hw + ux * hw, py - ry * hw + uy * hw, pz - rz * hw + uz * hw, 0, 1,
			px + rx * hw + ux * hw, py + ry * hw + uy * hw, pz + rz * hw + uz * hw, 1, 1,
		};

		glBindVertexArray(s_vao);
		glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
		glUniform1f(s_shaderMgr->GetUniformLoc(s_shader, "uAlpha"), p.alpha);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	}

	glBindVertexArray(0);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	glUseProgram(0);
}

// Exhaust stream
OGLExhaustStream::OGLExhaustStream(oapi::GraphicsClient *gc, PARTICLESTREAMSPEC *pss, ShaderMgr *shaderMgr,
                                    OBJHANDLE hVessel, const double *lvl, const VECTOR3 *ref, const VECTOR3 *dir)
	: OGLParticleStream(gc, pss, shaderMgr)
{
	Attach(hVessel, ref, dir, lvl);
}

// Reentry stream
OGLReentryStream::OGLReentryStream(oapi::GraphicsClient *gc, PARTICLESTREAMSPEC *pss, ShaderMgr *shaderMgr,
                                    OBJHANDLE hVessel)
	: OGLParticleStream(gc, pss, shaderMgr)
{
	// Reentry stream follows the vessel center
	VECTOR3 zero = {0, 0, 0};
	static double alwaysOn = 1.0;
	Attach(hVessel, &zero, &zero, &alwaysOn);
}

} // namespace ogl

#endif // !_WIN32
