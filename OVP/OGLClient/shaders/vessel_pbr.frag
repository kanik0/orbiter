#version 410 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUV;
in vec3 vSunDir;
in vec3 vViewDir;

// Material uniforms
uniform vec4 uDiffuse;
uniform vec3 uEmissive;
uniform vec4 uSpecular;     // rgb = specular color, a = power
uniform vec3 uReflect;      // reflectivity (F0)
uniform float uRoughness;   // 0 = smooth, 1 = rough
uniform float uMetalness;   // 0 = dielectric, 1 = metal

// Texture flags
uniform bool uHasDiffuseTex;
uniform bool uHasNormalMap;
uniform bool uHasSpecularMap;
uniform bool uHasEmissiveMap;
uniform bool uHasRoughnessMap;
uniform bool uHasMetalnessMap;
uniform bool uHasEnvMap;

// Texture samplers
uniform sampler2D uDiffuseTex;
uniform sampler2D uNormalMap;
uniform sampler2D uSpecularMap;
uniform sampler2D uEmissiveTex;
uniform sampler2D uRoughnessTex;
uniform sampler2D uMetalnessTex;
uniform samplerCube uEnvMap;

out vec4 FragColor;

const float PI = 3.14159265;

// --- PBR Functions ---

// GGX Normal Distribution Function
float DistributionGGX(float NdotH, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom + 0.0001);
}

// Schlick-GGX Geometry Function
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(float NdotV, float NdotL, float roughness) {
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

// Schlick Fresnel Approximation
vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    vec3 N = normalize(vNormal);
    vec3 V = normalize(vViewDir);
    vec3 L = normalize(vSunDir);
    vec3 H = normalize(V + L);

    // Normal mapping (tangent space → world space approximation)
    if (uHasNormalMap) {
        vec3 nm = texture(uNormalMap, vUV).rgb * 2.0 - 1.0;
        // Simple normal perturbation without explicit tangent frame
        // (full TBN requires tangent vectors in vertex data)
        vec3 up = abs(N.y) < 0.999 ? vec3(0, 1, 0) : vec3(1, 0, 0);
        vec3 T = normalize(cross(up, N));
        vec3 B = cross(N, T);
        N = normalize(T * nm.x + B * nm.y + N * nm.z);
    }

    // Material properties
    vec4 albedo = uDiffuse;
    if (uHasDiffuseTex) albedo *= texture(uDiffuseTex, vUV);

    float roughness = uRoughness;
    if (uHasRoughnessMap) roughness = texture(uRoughnessTex, vUV).g;
    roughness = clamp(roughness, 0.04, 1.0);

    float metalness = uMetalness;
    if (uHasMetalnessMap) metalness = texture(uMetalnessTex, vUV).g;

    vec3 emission = uEmissive;
    if (uHasEmissiveMap) emission = texture(uEmissiveTex, vUV).rgb;

    // F0: base reflectivity (0.04 for dielectrics, albedo for metals)
    vec3 F0 = mix(vec3(0.04), albedo.rgb, metalness);
    if (length(uReflect) > 0.01) F0 = uReflect;

    // Cook-Torrance BRDF
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.001);
    float NdotH = max(dot(N, H), 0.0);
    float HdotV = max(dot(H, V), 0.0);

    float D = DistributionGGX(NdotH, roughness);
    float G = GeometrySmith(NdotV, NdotL, roughness);
    vec3  F = FresnelSchlick(HdotV, F0);

    // Specular contribution
    vec3 numerator = D * G * F;
    float denominator = 4.0 * NdotV * NdotL + 0.0001;
    vec3 specular = numerator / denominator;

    // Energy conservation: diffuse is reduced for metals
    vec3 kD = (1.0 - F) * (1.0 - metalness);

    // Direct lighting (sun)
    vec3 Lo = (kD * albedo.rgb / PI + specular) * NdotL;

    // Ambient (simple constant)
    float ambient = 0.03;
    vec3 ambientColor = albedo.rgb * ambient * (1.0 - metalness * 0.5);

    // Environment reflections
    if (uHasEnvMap) {
        vec3 R = reflect(-V, N);
        float mipLevel = roughness * 6.0; // rough → blurred mip
        vec3 envColor = textureLod(uEnvMap, R, mipLevel).rgb;
        vec3 envFresnel = FresnelSchlick(NdotV, F0);
        ambientColor += envColor * envFresnel * (1.0 - roughness * 0.5);
    }

    // Specular highlight from legacy specular parameter
    if (!uHasRoughnessMap && uSpecular.a > 1.0) {
        // Legacy Blinn-Phong fallback when no PBR maps
        float spec = pow(max(dot(N, H), 0.0), uSpecular.a);
        Lo += uSpecular.rgb * spec * NdotL;
    }

    vec3 color = ambientColor + Lo + emission;

    // Simple tone mapping
    color = color / (color + 1.0);

    FragColor = vec4(color, albedo.a);
}
