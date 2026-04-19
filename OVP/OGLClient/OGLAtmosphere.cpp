// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef _WIN32

#include "OGLAtmosphere.h"
#include "OGLShaderMgr.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ogl {

// Earth baseline used to derive per-planet extinction. `color0` tints the
// radiance; the physical Rayleigh coefficients stay constant per unit density.
static constexpr float kEarthBetaR[3] = { 5.802e-6f, 13.558e-6f, 33.1e-6f };
static constexpr float kEarthBetaM    = 3.996e-6f;
static constexpr float kEarthScaleR   = 8500.0f;  // m
static constexpr float kEarthScaleM   = 1200.0f;  // m
static constexpr float kEarthAtmoAlt  = 84000.0f; // m

OGLAtmosphere::OGLAtmosphere(OBJHANDLE hPlanet, ShaderMgr *shaderMgr)
	: m_hPlanet(hPlanet), m_shaderMgr(shaderMgr), m_hasAtmo(false),
	  m_atmoAlt(0), m_horizonAlt(0), m_skyColor{0, 0, 0},
	  m_scaleR(kEarthScaleR), m_scaleM(kEarthScaleM),
	  m_betaR{kEarthBetaR[0], kEarthBetaR[1], kEarthBetaR[2]},
	  m_betaM(kEarthBetaM), m_mieG(0.76f),
	  m_sunRadiance{20.0f, 20.0f, 20.0f}, m_exposure(1.0f),
	  m_quadVAO(0), m_shader(0)
{
	m_hasAtmo = oapiPlanetHasAtmosphere(hPlanet);
	if (!m_hasAtmo) return;

	const ATMCONST *atm = oapiGetPlanetAtmConstants(hPlanet);
	if (atm) {
		// `altlimit` is Orbiter's physics cutoff (e.g. Earth: 2500 km) — far
		// above the visible atmosphere. `horizonalt` is the "visible top of
		// atmosphere" used by the legacy HazeMgr (e.g. Earth: 80 km,
		// Mars: 60 km) and is the right shell for single-scatter rendering.
		// Using altlimit inflated the shell 30× and spread density over km
		// that contribute nothing, leaving the integral imperceptible (#26).
		m_atmoAlt    = atm->horizonalt > 0 ? atm->horizonalt : atm->altlimit;
		m_horizonAlt = atm->horizonalt;
		m_skyColor   = atm->color0;
	}

	// Earth's real scale heights (~8.5 km Rayleigh, ~1.2 km Mie) are the
	// right defaults for the scatter integral; we tweak within a sane band
	// per body. The previous altRatio scaling anchored to altlimit flattened
	// the exponential falloff to useless levels (scaleR ≈ 250 km instead of
	// 8.5 km for Earth).
	const float altRatio = float(std::max(m_atmoAlt, 1000.0)) / kEarthAtmoAlt;
	const float scaleClamp = std::clamp(altRatio, 0.5f, 4.0f);
	m_scaleR = kEarthScaleR * scaleClamp;
	m_scaleM = kEarthScaleM * scaleClamp;

	// Tint the sun radiance by the module-provided sky colour so the dominant
	// wavelengths match (Orbiter's color0 already encodes the observed hue).
	// A neutral (1,1,1) sky leaves the default radiance untouched.
	const float avgSky = float((m_skyColor.x + m_skyColor.y + m_skyColor.z) / 3.0);
	if (avgSky > 1e-3f) {
		m_sunRadiance[0] *= float(m_skyColor.x) / avgSky;
		m_sunRadiance[1] *= float(m_skyColor.y) / avgSky;
		m_sunRadiance[2] *= float(m_skyColor.z) / avgSky;
	}

	// Denser shells (Venus' 90-bar, Titan's 1.5-bar) scatter harder — scale
	// the Mie coefficient, which dominates the near-horizon haze, by the
	// thickness ratio (clamped for the same reason as the scale heights).
	m_betaM = kEarthBetaM * scaleClamp;

	char name[64] = {0};
	oapiGetObjectName(hPlanet, name, 64);
	fprintf(stderr,
	        "[OGLAtmosphere] %s: altlimit=%.0fm scaleR=%.0fm scaleM=%.0fm "
	        "betaR=(%.2e,%.2e,%.2e) betaM=%.2e sky=(%.2f,%.2f,%.2f)\n",
	        name, m_atmoAlt, m_scaleR, m_scaleM,
	        m_betaR[0], m_betaR[1], m_betaR[2], m_betaM,
	        m_skyColor.x, m_skyColor.y, m_skyColor.z);

	m_shader = m_shaderMgr->LoadProgram("scatter", "scatter.vert", "scatter.frag");
	glGenVertexArrays(1, &m_quadVAO);  // empty VAO — vertices come from gl_VertexID
}

OGLAtmosphere::~OGLAtmosphere()
{
	if (m_quadVAO) glDeleteVertexArrays(1, &m_quadVAO);
}

void OGLAtmosphere::Render(const float *vp,
                           const VECTOR3 &camPos, const VECTOR3 &sunPos,
                           double planetRadius, const VECTOR3 &planetPos)
{
	if (!m_hasAtmo || !m_shader || !m_quadVAO) return;

	// Camera position in the planet-local frame, in absolute metres. The
	// scatter shader lives entirely in metres so it can compute exp(-h/H)
	// without precision gymnastics.
	const float camRel[3] = {
		float(camPos.x - planetPos.x),
		float(camPos.y - planetPos.y),
		float(camPos.z - planetPos.z)
	};
	(void)vp;  // legacy uniform: we now build rays from camera axes directly

	// Sun direction in the same frame — a world-frame unit vector is fine;
	// Orbiter's world axes are invariant under the planet translation.
	VECTOR3 sd = { sunPos.x - planetPos.x, sunPos.y - planetPos.y, sunPos.z - planetPos.z };
	double sdLen = std::sqrt(sd.x * sd.x + sd.y * sd.y + sd.z * sd.z);
	if (sdLen < 1e-6) return;
	const float sunDir[3] = {
		float(sd.x / sdLen), float(sd.y / sdLen), float(sd.z / sdLen)
	};

	// Camera basis directly, avoiding an in-shader inverse-VP reconstruction.
	// With Orbiter's far plane at 1e10 m the VP^-1 route crushes NDC into a
	// single ray direction under float32 precision — every pixel then sees
	// the atmosphere shell and the discard test fails (symptom: full-screen
	// atmosphere tint, #26). Building the ray from camera axes + fov keeps
	// every intermediate in a well-behaved range.
	MATRIX3 camRot;
	oapiCameraRotationMatrix(&camRot);
	const float camToWorld[9] = {
		(float)camRot.m11, (float)camRot.m21, (float)camRot.m31,  // camera +X in world
		(float)camRot.m12, (float)camRot.m22, (float)camRot.m32,  // camera +Y in world
		(float)camRot.m13, (float)camRot.m23, (float)camRot.m33,  // camera forward in world
	};
	const double fov       = oapiCameraAperture() * 2.0;
	const float  tanHalf   = (float)std::tan(fov * 0.5);
	GLint  vpDims[4];
	glGetIntegerv(GL_VIEWPORT, vpDims);
	const float  aspect    = (vpDims[3] > 0) ? (float)vpDims[2] / (float)vpDims[3] : 1.0f;

	const float atmoRadius = float(planetRadius + m_atmoAlt);
	const float planetR    = float(planetRadius);

	glUseProgram(m_shader);
	glUniformMatrix3fv(m_shaderMgr->GetUniformLoc(m_shader, "uCamToWorld"), 1, GL_FALSE, camToWorld);
	glUniform1f (m_shaderMgr->GetUniformLoc(m_shader, "uTanHalfFov"), tanHalf);
	glUniform1f (m_shaderMgr->GetUniformLoc(m_shader, "uAspect"),     aspect);
	glUniform3fv(m_shaderMgr->GetUniformLoc(m_shader, "uCamPosPlanet"), 1, camRel);
	glUniform1f (m_shaderMgr->GetUniformLoc(m_shader, "uPlanetRadius"), planetR);
	glUniform1f (m_shaderMgr->GetUniformLoc(m_shader, "uAtmoRadius"),   atmoRadius);
	glUniform3fv(m_shaderMgr->GetUniformLoc(m_shader, "uSunDir"),       1, sunDir);
	glUniform3fv(m_shaderMgr->GetUniformLoc(m_shader, "uSunRadiance"),  1, m_sunRadiance);
	glUniform3fv(m_shaderMgr->GetUniformLoc(m_shader, "uBetaR"),        1, m_betaR);
	glUniform1f (m_shaderMgr->GetUniformLoc(m_shader, "uBetaM"),        m_betaM);
	glUniform1f (m_shaderMgr->GetUniformLoc(m_shader, "uScaleR"),       m_scaleR);
	glUniform1f (m_shaderMgr->GetUniformLoc(m_shader, "uScaleM"),       m_scaleM);
	glUniform1f (m_shaderMgr->GetUniformLoc(m_shader, "uMieG"),         m_mieG);
	glUniform1f (m_shaderMgr->GetUniformLoc(m_shader, "uExposure"),     m_exposure);

	// Blend the atmosphere over whatever the planet surface wrote:
	//   FragColor = (inscatter, alpha)
	// src_alpha blending gives us: dst = src + dst * (1-alpha), which is the
	// standard "sky-over-surface" compositing with alpha = 1 - transmittance.
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	// Depth test off so the quad still paints haze on the planet surface
	// (closer than z=1) at the limb. Depth writes off so the fullscreen
	// quad's z=1 isn't smeared across the buffer — otherwise vessels
	// rendered later would fight a synthetic "far plane" depth at every
	// pixel the atmosphere touched (halo visibly overlapping the DG wing
	// along Earth's limb, #69 follow-on).
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glDisable(GL_CULL_FACE);

	glBindVertexArray(m_quadVAO);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray(0);

	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glUseProgram(0);
}

} // namespace ogl

#endif // !_WIN32
