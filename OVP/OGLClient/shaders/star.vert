#version 410 core
layout(location = 0) in vec3  aPos;
layout(location = 1) in float aBrightness;
layout(location = 2) in vec3  aColor;

uniform mat4  uViewProj;
uniform float uTime;

out float vBrightness;
out vec3  vColor;

void main() {
    gl_Position = uViewProj * vec4(aPos, 1.0);

    // Star twinkle — small amplitude, per-star phase derived from gl_VertexID.
    // Real stellar scintillation is an atmospheric effect, but the subtle
    // brightness wobble sells the night sky as a live thing rather than a
    // frozen texture.
    float phase = float(gl_VertexID) * 0.137;                    // golden-angle ~
    float wob   = 0.85 + 0.15 * sin(uTime * 2.3 + phase) *
                  cos(uTime * 0.71 + phase * 0.5);
    float b     = clamp(aBrightness * wob, 0.0, 1.0);

    // Point-sprite size scales with brightness. Clamp so dim stars still
    // render at 1 pixel rather than disappearing between frames.
    gl_PointSize = max(1.5, b * 5.0);
    vBrightness  = b;
    vColor       = aColor;
}
