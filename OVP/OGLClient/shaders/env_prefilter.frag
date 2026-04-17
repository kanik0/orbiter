#version 410 core
#include "common.glsl"
#include "include/brdf.glsl.inc"

// GGX-importance-sampled pre-filter of the capture cubemap, one mip per
// roughness level. Output feeds vessel_pbr.frag's iblSpecular lookup via
// samplerCube uEnvMap + textureLod(..., roughness * 4).

in vec2 vNdc;

uniform int         uFace;
uniform float       uRoughness;
uniform samplerCube uCapture;
uniform int         uSamples;   // 16 for mip0, up to 64 for higher mips

out vec4 FragColor;

// Shared face-direction map. Keeping this identical to env_capture.frag
// means the first mip is visually congruent with the source.
vec3 faceDir(int face, vec2 uv) {
    vec3 d;
    if      (face == 0) d = vec3( 1.0, -uv.y, -uv.x);
    else if (face == 1) d = vec3(-1.0, -uv.y,  uv.x);
    else if (face == 2) d = vec3( uv.x,  1.0,  uv.y);
    else if (face == 3) d = vec3( uv.x, -1.0, -uv.y);
    else if (face == 4) d = vec3( uv.x, -uv.y,  1.0);
    else                d = vec3(-uv.x, -uv.y, -1.0);
    return normalize(d);
}

// Van der Corput + Hammersley — standard quasi-random sequence for
// importance sampling.
float radicalInverse(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

vec2 hammersley(uint i, uint n) {
    return vec2(float(i) / float(n), radicalInverse(i));
}

vec3 importanceSampleGGX(vec2 Xi, vec3 N, float a) {
    float a2 = a * a;
    float phi      = TWO_PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a2 * a2 - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    vec3 H = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);

    vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 T  = normalize(cross(up, N));
    vec3 B  = cross(N, T);
    return normalize(T * H.x + B * H.y + N * H.z);
}

void main() {
    vec3 N = faceDir(uFace, vNdc);
    vec3 V = N; // split-sum approximation

    // Roughness 0 → no convolution, copy the source mip directly.
    if (uRoughness < 1e-3) {
        FragColor = vec4(textureLod(uCapture, N, 0.0).rgb, 1.0);
        return;
    }

    vec3  color  = vec3(0.0);
    float weight = 0.0;
    uint  n      = uint(uSamples);

    for (uint i = 0u; i < n; ++i) {
        vec2 Xi = hammersley(i, n);
        vec3 H  = importanceSampleGGX(Xi, N, uRoughness);
        vec3 L  = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0) {
            color  += textureLod(uCapture, L, 0.0).rgb * NdotL;
            weight += NdotL;
        }
    }

    FragColor = vec4(color / max(weight, 1e-4), 1.0);
}
