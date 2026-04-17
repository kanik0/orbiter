#version 410 core
#include "include/material.glsl.inc"

in float vLight;
in vec3 vNormal;
in vec2 vUV;

// Samplers must stay outside the UBO (std140 disallows opaque types).
uniform sampler2D uTexture;

out vec4 FragColor;

void main() {
    vec4 baseColor = matDiffuse;
    if (matHasDiffuse != 0) {
        baseColor *= texture(uTexture, vUV);
    }
    vec3 lit = baseColor.rgb * vLight + matEmissive.rgb;
    FragColor = vec4(lit, baseColor.a);
}
