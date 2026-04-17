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
in vec3 vTangent;
in vec3 vBitangent;

// Samplers must live outside the Material UBO (std140 disallows opaque types).
uniform sampler2D  uDiffuseTex;
uniform sampler2D  uNormalMap;
uniform sampler2D  uSpecularMap;
uniform sampler2D  uEmissiveTex;
uniform sampler2D  uRoughnessTex;
uniform sampler2D  uMetalnessTex;
uniform samplerCube uEnvMap;

out vec4 FragColor;

// Build a per-fragment TBN. Uses the real per-vertex tangent when available;
// falls back to a cheap derivative approximation otherwise so the shader
// still renders something sensible on meshes that skipped the tangent pass.
mat3 pickTBN(vec3 N) {
    if (matHasTangent != 0) {
        vec3 T = normalize(vTangent);
        vec3 B = normalize(vBitangent);
        return mat3(T, B, N);
    }
    vec3 dp1 = dFdx(vWorldPos);
    vec3 dp2 = dFdy(vWorldPos);
    vec2 du1 = dFdx(vUV);
    vec2 du2 = dFdy(vUV);
    vec3 T = normalize(dp1 * du2.y - dp2 * du1.y);
    vec3 B = normalize(cross(N, T));
    return mat3(T, B, N);
}

void main() {
    vec3 N = normalize(vNormal);
    vec3 V = normalize(vViewDir);
    vec3 L = normalize(vSunDir);
    mat3 TBN = pickTBN(N);

    // Normal mapping via the full TBN frame.
    if (matHasNormal != 0) {
        vec3 nm = texture(uNormalMap, vUV).rgb * 2.0 - 1.0;
        N = normalize(TBN * nm);
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

    vec3 F0 = materialF0(albedo.rgb);

    // --- Base Cook-Torrance lobe ---
    float NdotL;
    vec3  F;
    vec3  specular = cookTorranceSpec(N, V, L, F0, roughness, NdotL, F);
    vec3  kD = (1.0 - F) * (1.0 - metalness);
    vec3  Lo = (kD * albedo.rgb * INV_PI + specular) * NdotL;

    // --- Clearcoat lobe ---
    // A thin dielectric film (F0 = 0.04) that sits on top of the base
    // material. DeltaGlider-style canopies and painted fuselages use this
    // to add a soft top-coat highlight without doubling specular energy.
    if (matClearcoat > 0.0) {
        vec3  ccH     = normalize(V + L);
        float ccNdotH = max(dot(N, ccH), 0.0);
        float ccNdotV = max(dot(N, V),   1e-3);
        float ccHdotV = max(dot(ccH, V), 0.0);
        float ccR     = clamp(matClearcoatRoughness, 0.04, 1.0);

        float ccD = distGGX(ccNdotH, ccR);
        float ccG = geomSmith(ccNdotV, NdotL, ccR);
        vec3  ccF = fresnelSchlick(ccHdotV, vec3(0.04));
        vec3  ccSpec = (ccD * ccG * ccF) / (4.0 * ccNdotV * NdotL + 1e-4);

        // Energy borrowed from the base lobe so the combined highlight
        // stays plausible on strongly coated materials.
        vec3 attenuation = vec3(1.0) - ccF * matClearcoat;
        Lo = Lo * attenuation + ccSpec * matClearcoat * NdotL;
    }

    // --- Ambient / IBL ---
    float ambientFloor = 0.03;
    vec3  ambientColor = albedo.rgb * ambientFloor * (1.0 - metalness * 0.5);
    if (matHasEnvMap != 0) {
        vec3 R = reflect(-V, N);
        vec3 envColor = iblSpecular(uEnvMap, R, roughness);
        float NdotV   = max(dot(N, V), 1e-3);
        vec3 envF     = fresnelSchlick(NdotV, F0);
        ambientColor += envColor * envF * (1.0 - roughness * 0.5);
    }

    // Legacy Blinn-Phong highlight for meshes without PBR maps.
    if (matHasRoughness == 0 && matSpecular.a > 1.0) {
        float specPhong = blinnPhongSpec(N, L, V, matSpecular.a);
        Lo += matSpecular.rgb * specPhong * NdotL;
    }

    vec3 color = ambientColor + Lo + emission;
    color = color / (color + 1.0);  // Reinhard placeholder until M11 HDR

    FragColor = vec4(color, albedo.a);
}
