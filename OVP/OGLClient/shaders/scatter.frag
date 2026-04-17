#version 410 core
#include "common.glsl"
#include "include/scatter_common.glsl.inc"

// Atmospheric single-scattering evaluated per pixel on a fullscreen pass.
//
// The caller supplies the inverse view-projection matrix of the render's
// distance-normalised camera (used only to derive the view direction — it
// does not need to be physically scaled), plus all atmosphere parameters in
// metres relative to the planet centre. The vertex shader renders a
// fullscreen quad at z=1; we discard anything that misses the atmosphere
// shell so the planet surface underneath remains untouched.

in vec2 vNdc;

uniform mat4 uViewProj;       // same VP the planet surface pass used
uniform vec3 uCamPosPlanet;   // camera position in planet-local frame [m]
uniform float uPlanetRadius;  // planet radius [m]
uniform float uAtmoRadius;    // planet radius + atmosphere altitude [m]
uniform vec3 uSunDir;         // unit vector, planet-local
uniform vec3 uSunRadiance;    // sun spectral radiance (pre-exposure)
uniform vec3 uBetaR;          // Rayleigh extinction coeffs, per-wavelength [m^-1]
uniform float uBetaM;         // Mie extinction [m^-1]
uniform float uScaleR;        // Rayleigh scale height [m]
uniform float uScaleM;        // Mie scale height [m]
uniform float uMieG;          // Henyey-Greenstein asymmetry
uniform float uExposure;      // atmosphere-only exposure multiplier

out vec4 FragColor;

// Reconstruct a unit world-space view direction from NDC using the inverse VP.
vec3 ndcToWorldDir(mat4 invVP, vec2 ndc) {
    vec4 nh = invVP * vec4(ndc, -1.0, 1.0);
    vec4 fh = invVP * vec4(ndc,  1.0, 1.0);
    vec3 n  = nh.xyz / nh.w;
    vec3 f  = fh.xyz / fh.w;
    return normalize(f - n);
}

void main() {
    mat4 invVP = inverse(uViewProj);
    vec3 rd = ndcToWorldDir(invVP, vNdc);
    vec3 ro = uCamPosPlanet;
    vec3 C  = vec3(0.0);

    // Test atmosphere shell. Miss → nothing to do.
    vec2 tAtmo = raySphere(ro, rd, C, uAtmoRadius);
    if (tAtmo.y <= 0.0) { discard; }

    // If the ray also hits the planet, clamp the integration there so the
    // already-rendered surface receives only the in-scattered sky plus
    // whatever transmittance we leave in the alpha.
    vec2 tPlanet = raySphere(ro, rd, C, uPlanetRadius);
    float rayLen = tAtmo.y;
    if (tPlanet.x > 0.0) rayLen = min(rayLen, tPlanet.x);

    vec3 inscatter, transmittance;
    scatterAlongRay(
        ro, rd, rayLen,
        C,
        uPlanetRadius, uAtmoRadius,
        uSunDir, uSunRadiance,
        uBetaR, uBetaM,
        uScaleR, uScaleM, uMieG,
        16, 6,
        inscatter, transmittance);

    // Soft tone-map the in-scatter only so that the sun disc doesn't clip; a
    // global HDR pass lands in M11 and will subsume this.
    vec3 color = 1.0 - exp(-inscatter * uExposure);

    // Alpha is the fraction of the backdrop *occluded* by the atmosphere.
    // Behind-atmosphere pixels (space, stars) multiply by transmittance;
    // surface pixels (planet) already had their own shading and receive the
    // same attenuation plus the in-scatter.
    float alpha = clamp(1.0 - dot(transmittance, vec3(0.333)), 0.0, 1.0);

    FragColor = vec4(color, alpha);
}
