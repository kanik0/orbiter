// brdf.glsl.inc — Cook-Torrance microfacet BRDF.
//
// Extracted from vessel_pbr.frag so M8 (PBR vessel parity) can extend the
// model (anisotropic, clearcoat) without touching every call site.

#pragma once

// GGX Normal Distribution.
float distGGX(float NdotH, float roughness) {
    float a  = roughness * roughness;
    float a2 = a * a;
    float d  = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d + 1e-4);
}

// Schlick-GGX geometry term, per direction.
float geomSchlickGGX(float NdotX, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotX / (NdotX * (1.0 - k) + k);
}

// Smith-joint geometry.
float geomSmith(float NdotV, float NdotL, float roughness) {
    return geomSchlickGGX(NdotV, roughness) * geomSchlickGGX(NdotL, roughness);
}

// Schlick Fresnel.
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Full Cook-Torrance specular lobe. Returns the specular reflectance;
// combine with `(1-F)(1-metal) * albedo / PI` for the diffuse term.
vec3 cookTorranceSpec(vec3 N, vec3 V, vec3 L, vec3 F0, float roughness,
                      out float NdotL, out vec3 F)
{
    vec3  H     = normalize(V + L);
    NdotL       = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 1e-3);
    float NdotH = max(dot(N, H), 0.0);
    float HdotV = max(dot(H, V), 0.0);

    float D = distGGX(NdotH, roughness);
    float G = geomSmith(NdotV, NdotL, roughness);
    F       = fresnelSchlick(HdotV, F0);

    return (D * G * F) / (4.0 * NdotV * NdotL + 1e-4);
}
