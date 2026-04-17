// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef _WIN32

#include "OGLShadowMap.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace ogl {

OGLShadowMap::OGLShadowMap()
	: m_fbo(0), m_depthTex(0), m_size(0),
	  m_prevFBO(0)
{
	std::memset(m_lightVP, 0, sizeof(m_lightVP));
	m_lightVP[0] = m_lightVP[5] = m_lightVP[10] = m_lightVP[15] = 1.0f;
	m_prevViewport[0] = m_prevViewport[1] = 0;
	m_prevViewport[2] = m_prevViewport[3] = 0;
}

OGLShadowMap::~OGLShadowMap() { Release(); }

bool OGLShadowMap::Init(int size)
{
	m_size = size;

	glGenTextures(1, &m_depthTex);
	glBindTexture(GL_TEXTURE_2D, m_depthTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24,
	             size, size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	// Clamp to a white (= "far", fully lit) border so fragments outside the
	// shadow frustum aren't marked as shadowed.
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	const float border[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
	glBindTexture(GL_TEXTURE_2D, 0);

	glGenFramebuffers(1, &m_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
	                       GL_TEXTURE_2D, m_depthTex, 0);
	// Depth-only FBO: disable colour reads / writes.
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);

	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	if (status != GL_FRAMEBUFFER_COMPLETE) {
		fprintf(stderr, "[OGLShadowMap] FBO incomplete: 0x%x\n", status);
		Release();
		return false;
	}

	fprintf(stderr, "[OGLShadowMap] %dx%d depth24 ready\n", m_size, m_size);
	return true;
}

void OGLShadowMap::Release()
{
	if (m_fbo)      { glDeleteFramebuffers(1, &m_fbo);    m_fbo = 0; }
	if (m_depthTex) { glDeleteTextures(1, &m_depthTex);   m_depthTex = 0; }
	m_size = 0;
}

// Column-major 4x4 multiply: out = a * b.
static void mat4Mul(const float *a, const float *b, float *out)
{
	for (int c = 0; c < 4; c++) {
		for (int r = 0; r < 4; r++) {
			out[c * 4 + r] =
				a[0 * 4 + r] * b[c * 4 + 0] +
				a[1 * 4 + r] * b[c * 4 + 1] +
				a[2 * 4 + r] * b[c * 4 + 2] +
				a[3 * 4 + r] * b[c * 4 + 3];
		}
	}
}

void OGLShadowMap::BuildLightMatrix(const VECTOR3 &sunDir, const VECTOR3 &targetPos,
                                    double halfExtent, double sunDist, double farDist)
{
	// Light is at (targetPos + sunDir * sunDist), looking toward targetPos.
	// Build an orthonormal basis (right, up, forward) with forward = -sunDir.
	double sx = sunDir.x, sy = sunDir.y, sz = sunDir.z;
	double sl = std::sqrt(sx * sx + sy * sy + sz * sz);
	if (sl < 1e-9) { sx = 0; sy = 1; sz = 0; sl = 1; }
	sx /= sl; sy /= sl; sz /= sl;

	// pick any up vector not parallel to sunDir
	double ux = 0, uy = 1, uz = 0;
	if (std::fabs(sy) > 0.95) { ux = 1; uy = 0; uz = 0; }

	// right = normalize(cross(up, forward))   (forward = -sunDir so cross(up,-s))
	double fx = -sx, fy = -sy, fz = -sz;
	double rx = uy * fz - uz * fy;
	double ry = uz * fx - ux * fz;
	double rz = ux * fy - uy * fx;
	double rl = std::sqrt(rx * rx + ry * ry + rz * rz);
	rx /= rl; ry /= rl; rz /= rl;
	// up' = cross(forward, right)
	ux = fy * rz - fz * ry;
	uy = fz * rx - fx * rz;
	uz = fx * ry - fy * rx;

	double eyeX = targetPos.x + sx * sunDist;
	double eyeY = targetPos.y + sy * sunDist;
	double eyeZ = targetPos.z + sz * sunDist;

	// View matrix (world → light-space) column-major.
	float V[16];
	V[0] = float(rx);  V[4] = float(ry);  V[8]  = float(rz);  V[12] = float(-(rx*eyeX + ry*eyeY + rz*eyeZ));
	V[1] = float(ux);  V[5] = float(uy);  V[9]  = float(uz);  V[13] = float(-(ux*eyeX + uy*eyeY + uz*eyeZ));
	V[2] = float(fx);  V[6] = float(fy);  V[10] = float(fz);  V[14] = float(-(fx*eyeX + fy*eyeY + fz*eyeZ));
	V[3] = 0;          V[7] = 0;          V[11] = 0;          V[15] = 1;

	// Orthographic projection: [-halfExtent, +halfExtent] horizontally and
	// vertically, depth from 0 to farDist along the light's -Z.
	double L = -halfExtent, R = halfExtent;
	double B = -halfExtent, T = halfExtent;
	double N = 0.0,        F = farDist;

	float P[16] = {0};
	P[0]  = float(2.0 / (R - L));
	P[5]  = float(2.0 / (T - B));
	P[10] = float(-2.0 / (F - N));
	P[12] = float(-(R + L) / (R - L));
	P[13] = float(-(T + B) / (T - B));
	P[14] = float(-(F + N) / (F - N));
	P[15] = 1.0f;

	mat4Mul(P, V, m_lightVP);
}

void OGLShadowMap::BeginPass()
{
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &m_prevFBO);
	glGetIntegerv(GL_VIEWPORT, m_prevViewport);

	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
	glViewport(0, 0, m_size, m_size);
	glClear(GL_DEPTH_BUFFER_BIT);

	// Slope-scaled polygon offset keeps self-shadow acne off the lit
	// surface without pushing the silhouette edge into Peter Pan.
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(1.5f, 4.0f);
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
}

void OGLShadowMap::EndPass()
{
	glDisable(GL_POLYGON_OFFSET_FILL);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)m_prevFBO);
	glViewport(m_prevViewport[0], m_prevViewport[1],
	           m_prevViewport[2], m_prevViewport[3]);
}

} // namespace ogl

#endif // !_WIN32
