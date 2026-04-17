// common.glsl — shared constants and lighting primitives for OGLClient shaders.
//
// Meant to be pulled in via:    #include "common.glsl"
// (resolved by OGLShaderMgr either at <shaderPath>/common.glsl or at
//  <shaderPath>/include/common.glsl).
//
// Reusable modules live under shaders/include/*.glsl.inc:
//   - material.glsl.inc  Material uniform block + helpers.
//   - brdf.glsl.inc      Cook-Torrance GGX/Schlick/Smith.
//   - ibl.glsl.inc       Cubemap sampling for diffuse+specular IBL.
//   - gamma.glsl.inc     sRGB ↔ linear helpers.
//   - scatter_common.glsl.inc  Rayleigh/Mie constants (filled in M4).

#pragma once

// --- Numerical constants -----------------------------------------------------
#ifndef PI
#define PI 3.14159265358979
#endif
#ifndef TWO_PI
#define TWO_PI 6.28318530717958
#endif
#ifndef INV_PI
#define INV_PI 0.31830988618379
#endif
#ifndef EPSILON
#define EPSILON 1e-6
#endif

// --- Light description -------------------------------------------------------
// Shader code that does not yet use the Light UBO can still consume the sun
// via a plain vec3 uniform; this struct exists so feature-rich shaders can
// share helpers without redeclaring parameter lists.
struct DirectionalLight {
    vec3  direction;    // normalised, world space, pointing *from* the light
    vec3  color;        // linear-space RGB radiance
    float ambient;      // [0,1] bottom-floor for unshaded surfaces
};

// --- Core lighting primitives ------------------------------------------------

// Classic Lambert with an ambient floor — used by the simple vessel/planet
// pipelines before they migrate to PBR.
float sunLight(vec3 worldNormal, vec3 sunDir, float ambient) {
    return max(ambient, dot(worldNormal, sunDir));
}

float sunLight(vec3 worldNormal, DirectionalLight L) {
    return max(L.ambient, dot(worldNormal, L.direction));
}

// Blinn-Phong specular lobe, still handy for panel instruments and
// cockpit glass highlights where PBR is overkill.
float blinnPhongSpec(vec3 N, vec3 L, vec3 V, float shininess) {
    vec3  H     = normalize(L + V);
    float NdotH = max(dot(N, H), 0.0);
    return pow(NdotH, shininess);
}

// Two-sided Lambert: softens the dark side instead of clipping to zero.
// Useful for clouds and thin-shell atmospheres.
float wrapLambert(vec3 N, vec3 L, float wrap) {
    float NdotL = dot(N, L);
    return max(0.0, (NdotL + wrap) / (1.0 + wrap));
}
