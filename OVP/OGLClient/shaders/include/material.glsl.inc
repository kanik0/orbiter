// material.glsl.inc — PBR material description.
//
// Binding point is the `Material` UBO (slot 2 in ogl::UBO::Material).
// OGLShaderMgr::BindUBOBlocks() wires the layout(std140, binding=2) block
// automatically whenever a shader declares a block named `Material`.
//
// Layout mirrors OGLvVessel's upload path; any change here must be kept
// in sync with the CPU-side struct in OGLvVessel.cpp.

#pragma once

layout(std140) uniform Material {
    vec4  matDiffuse;     // rgb = albedo, a = alpha
    vec4  matSpecular;    // rgb = spec colour, a = power (legacy Blinn-Phong)
    vec4  matEmissive;    // rgb = emissive radiance, a unused
    vec4  matReflect;     // rgb = F0 override, a unused
    float matRoughness;   // 0 smooth … 1 rough
    float matMetalness;   // 0 dielectric … 1 metal
    float matOpacity;     // 0 invisible … 1 opaque
    float matFresnelPow;  // legacy fresnel exponent
    // Texture-presence flags packed as ints (std140 pads so one int per slot).
    int   matHasDiffuse;
    int   matHasNormal;
    int   matHasSpecular;
    int   matHasEmissive;
    int   matHasRoughness;
    int   matHasMetalness;
    int   matHasEnvMap;
    int   matPad0;
};

// Compute F0 with the standard "albedo for metals, 0.04 for dielectrics"
// rule, overridden by matReflect when the caller provides an explicit
// reflectivity (useful for hand-tuned glass/canopy materials).
vec3 materialF0(vec3 albedo) {
    vec3 baseF0 = mix(vec3(0.04), albedo, float(matMetalness));
    return any(greaterThan(matReflect.rgb, vec3(0.01))) ? matReflect.rgb : baseF0;
}
