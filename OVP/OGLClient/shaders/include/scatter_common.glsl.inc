// scatter_common.glsl.inc — Rayleigh/Mie single-scattering library.
//
// The atmosphere model is a classical Nishita-93 single-scatter implementation
// evaluated analytically per pixel (no 3D LUT). It handles both the in-scatter
// term (sky glow) and the extinction term (aerial perspective / attenuation of
// background surfaces).
//
// Call convention: every length is in metres in the planet-local frame.
// `planetCenter` is the planet's centre, usually vec3(0). Ray origin is the
// camera position in the same frame.
//
// Upstream shaders usually also want:
//   #include "common.glsl"   // PI, INV_PI

#pragma once

// Standard Earth baseline, used as a reference; per-body coefficients are
// passed in as uniforms on top of these.
const vec3  SCATTER_EARTH_BETA_R = vec3(5.802e-6, 13.558e-6, 33.1e-6); // m^-1
const float SCATTER_EARTH_BETA_M = 3.996e-6;                           // m^-1
const float SCATTER_EARTH_H_R    = 8500.0;                             // m
const float SCATTER_EARTH_H_M    = 1200.0;                             // m
const float SCATTER_DEFAULT_G    = 0.76;                               // Mie HG

// --- Phase functions ---------------------------------------------------------

float rayleighPhase(float mu) {
	return 3.0 / (16.0 * PI) * (1.0 + mu * mu);
}

float miePhaseHG(float mu, float g) {
	float g2 = g * g;
	float num = (1.0 - g2) * (1.0 + mu * mu);
	float den = (2.0 + g2) * pow(max(1.0 + g2 - 2.0 * g * mu, 1e-4), 1.5);
	return 3.0 / (8.0 * PI) * num / den;
}

// --- Ray-sphere intersection (planet / atmosphere boundary) ------------------

// Returns (t_near, t_far). A miss yields (-1, -1); t_near may be negative when
// the origin is inside the sphere, in which case t_far is the exit distance.
vec2 raySphere(vec3 ro, vec3 rd, vec3 center, float radius) {
	vec3  oc = ro - center;
	float b  = dot(oc, rd);
	float c  = dot(oc, oc) - radius * radius;
	float h  = b * b - c;
	if (h < 0.0) return vec2(-1.0);
	h = sqrt(h);
	return vec2(-b - h, -b + h);
}

// Exponential atmosphere density, clamped so `h < 0` (below the reference
// surface — possible from float noise near the planet) returns the sea-level
// value rather than blowing up.
float densityExp(float h, float scaleH) {
	return exp(-max(h, 0.0) / scaleH);
}

// --- Optical depth from a point to the outer atmosphere ---------------------

// Accumulates optical depth along the ray (p, dir) out to the atmosphere
// shell. `steps` samples of the two density terms (Rayleigh + Mie) are taken.
// If the ray never enters the shell (sun below horizon from p) both outputs
// are set to a large sentinel, producing near-zero transmittance.
void scatterOpticalDepthR(
	vec3 p, vec3 dir, vec3 planetCenter,
	float planetRadius, float atmoRadius,
	float scaleR, float scaleM,
	int steps, out float odR, out float odM)
{
	vec2 t = raySphere(p, dir, planetCenter, atmoRadius);
	float tmax = t.y;
	if (tmax <= 0.0) {
		odR = odM = 1e9;
		return;
	}
	tmax = max(tmax, 0.0);

	float ds = tmax / float(steps);
	odR = 0.0;
	odM = 0.0;
	for (int i = 0; i < steps; i++) {
		vec3 q = p + dir * ((float(i) + 0.5) * ds);
		float h = length(q - planetCenter) - planetRadius;
		odR += densityExp(h, scaleR) * ds;
		odM += densityExp(h, scaleM) * ds;
	}
}

// --- View-ray integration ---------------------------------------------------

// Integrates single-scattering along the view ray within the atmosphere shell.
// If the ray clips the planet surface before exiting, `rayLength` should be
// set to the planet-hit distance so the integral stops there (the remaining
// scatter is covered by surface illumination).
//
// Outputs:
//   inscatter      — RGB radiance added by the sky
//   transmittance  — attenuation of whatever lies behind the atmosphere
void scatterAlongRay(
	vec3 ro, vec3 rd, float rayLength,
	vec3 planetCenter,
	float planetRadius, float atmoRadius,
	vec3 sunDir, vec3 sunRadiance,
	vec3 betaR, float betaM,
	float scaleR, float scaleM, float mieG,
	int stepsView, int stepsSun,
	out vec3 inscatter, out vec3 transmittance)
{
	vec2 t = raySphere(ro, rd, planetCenter, atmoRadius);
	if (t.y <= 0.0) {
		inscatter     = vec3(0.0);
		transmittance = vec3(1.0);
		return;
	}
	float tStart = max(t.x, 0.0);
	float tEnd   = min(t.y, rayLength);
	if (tEnd <= tStart) {
		inscatter     = vec3(0.0);
		transmittance = vec3(1.0);
		return;
	}

	float ds = (tEnd - tStart) / float(stepsView);
	vec3  P0 = ro + rd * tStart;

	vec3 sumR = vec3(0.0);
	vec3 sumM = vec3(0.0);
	float odR = 0.0, odM = 0.0;

	for (int i = 0; i < stepsView; i++) {
		vec3 p = P0 + rd * ((float(i) + 0.5) * ds);
		float h = length(p - planetCenter) - planetRadius;
		float dR = densityExp(h, scaleR) * ds;
		float dM = densityExp(h, scaleM) * ds;
		odR += dR;
		odM += dM;

		float sOdR, sOdM;
		scatterOpticalDepthR(p, sunDir, planetCenter, planetRadius, atmoRadius,
		                     scaleR, scaleM, stepsSun, sOdR, sOdM);

		vec3 tau = betaR * (odR + sOdR)
		         + vec3(betaM) * 1.1 * (odM + sOdM);
		vec3 att = exp(-tau);
		sumR += dR * att;
		sumM += dM * att;
	}

	float mu     = dot(rd, sunDir);
	float phaseR = rayleighPhase(mu);
	float phaseM = miePhaseHG(mu, mieG);

	inscatter     = sunRadiance * (betaR * phaseR * sumR + vec3(betaM) * phaseM * sumM);
	transmittance = exp(-(betaR * odR + vec3(betaM) * 1.1 * odM));
}
