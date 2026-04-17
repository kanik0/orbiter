#version 410 core
// Cloud sphere — shares the texplanet mesh layout but rendered at a slightly
// larger radius than the planet surface so clouds sit above it.

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

uniform mat4 uViewProj;
uniform mat4 uModel;
uniform vec3 uSunDir;      // world-space, unit
uniform float uUVOffset;   // longitudinal drift (simulation time driven)

out vec3 vNormal;
out vec2 vUV;
out vec3 vViewDir;         // from fragment toward camera, world-space
out float vSunDot;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    gl_Position   = uViewProj * worldPos;

    // mat3(uModel) is uniform scale, so its inverse-transpose equals itself
    // up to the common factor — fine for directional lighting.
    vNormal = normalize(mat3(uModel) * aNormal);

    // Differential rotation: shift the U coordinate with the sim clock so
    // clouds drift relative to the terrain below them.
    vUV = vec2(aUV.x + uUVOffset, aUV.y);

    vSunDot  = dot(vNormal, uSunDir);

    // View direction for Fresnel horizon fade — the VP already accounts for
    // the camera being at the origin in the distance-normalised frame.
    vec3 worldFromEye = worldPos.xyz; // camera at origin
    vViewDir = normalize(-worldFromEye);
}
