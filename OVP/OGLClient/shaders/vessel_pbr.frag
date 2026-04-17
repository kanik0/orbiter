#version 410 core
#include "common.glsl"
#include "include/material.glsl.inc"
#include "include/brdf.glsl.inc"
#include "include/ibl.glsl.inc"

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUV;
in vec3 vSunDir;
in vec3 vViewDir;

// Samplers must live outside the Material UBO (std140 disallows opaque types).
uniform sampler2D  uDiffuseTex;
uniform sampler2D  uNormalMap;
uniform sampler2D  uSpecularMap;
uniform sampler2D  uEmissiveTex;
uniform sampler2D  uRoughnessTex;
uniform sampler2D  uMetalnessTex;
uniform samplerCube uEnvMap;

out vec4 FragColor;

void main() {
    vec3 N = normalize(vNormal);
    vec3 V = normalize(vViewDir);
    vec3 L = normalize(vSunDir);

    // Normal mapping (world-space approximation without a tangent frame).
    if (matHasNormal != 0) {
        vec3 nm = texture(uNormalMap, vUV).rgb * 2.0 - 1.0;
        vec3 up = abs(N.y) < 0.999 ? vec3(0, 1, 0) : vec3(1, 0, 0);
        vec3 T = normalize(cross(up, N));
        vec3 B = cross(N, T);
        N = normalize(T * nm.x + B * nm.y + N * nm.z);
    }

    vec4 albedo = matDiffuse;
    if (matHasDiffuse != 0) albedo *= texture(uDiffuseTex, vUV);

    float roughness = matRoughness;
    if (matHasRoughness != 0) roughness = texture(uRoughnessTex, vUV).g;
    roughness = clamp(roughness, 0.04, 1.0);

    float metalness = matMetalness;
    if (matHasMetalness != 0) metalness = texture(uMetalnessTex, vUV).g;
    metalness = clamp(metalness, 0.0, 1.0);

    vec3 emission = matEmissive.rgb;
    if (matHasEmissive != 0) emission = texture(uEmissiveTex, vUV).rgb;

    // F0: 0.04 for dielectrics, albedo for metals; honour explicit override.
    vec3 F0 = materialF0(albedo.rgb);

    // Cook-Torrance direct lighting.
    float NdotL;
    vec3  F;
    vec3 specular = cookTorranceSpec(N, V, L, F0, roughness, NdotL, F);

    // Diffuse Lambert with energy-conserving metal suppression.
    vec3 kD = (1.0 - F) * (1.0 - metalness);
    vec3 Lo = (kD * albedo.rgb * INV_PI + specular) * NdotL;

    // Ambient + IBL.
    float ambientFloor = 0.03;
    vec3 ambientColor  = albedo.rgb * ambientFloor * (1.0 - metalness * 0.5);
    if (matHasEnvMap != 0) {
        vec3 R = reflect(-V, N);
        vec3 envColor = iblSpecular(uEnvMap, R, roughness);
        float NdotV   = max(dot(N, V), 1e-3);
        vec3 envF     = fresnelSchlick(NdotV, F0);
        ambientColor += envColor * envF * (1.0 - roughness * 0.5);
    }

    // Legacy Blinn-Phong highlight when the mesh has no PBR maps but carries
    // a non-trivial specular power (keeps older meshes from looking flat).
    if (matHasRoughness == 0 && matSpecular.a > 1.0) {
        float specPhong = blinnPhongSpec(N, L, V, matSpecular.a);
        Lo += matSpecular.rgb * specPhong * NdotL;
    }

    vec3 color = ambientColor + Lo + emission;

    // Simple Reinhard tone map — will be replaced by the HDR pipeline in M11.
    color = color / (color + 1.0);

    FragColor = vec4(color, albedo.a);
}
