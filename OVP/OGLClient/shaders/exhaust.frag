#version 410 core
// Exhaust plume billboard. The texture atlas packs the plume gradient along
// U (u≈0.24 = nozzle root = hot core; u≈0.01 = far tail = cool fade). We
// mix a warm inner tone over a hotter core colour based on U so the plume
// renders as a genuine bicolor jet instead of a flat orange stripe, and
// soften the V edges so the quad silhouette doesn't read as a hard box.

in vec2 vUV;

uniform sampler2D uTexture;
uniform float     uAlpha;
uniform float     uTime;   // seconds, drives the animated flicker
uniform float     uPhase;  // per-thruster phase offset so engines don't pulse in unison

out vec4 FragColor;

// Cheap hash-based noise, UV-seeded, for per-fragment turbulence.
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    vec4 tex = texture(uTexture, vUV);

    // Combined brightness flicker: a smooth ~20 Hz sinusoid plus a small
    // amount of UV-seeded turbulence scrolled along the plume axis. Keeps
    // the plume alive without looking stroboscopic.
    float pulse = 0.90 + 0.10 * sin(uTime * 22.0 + uPhase);
    float turb  = 0.95 + 0.10 * hash(vec2(vUV.y * 7.0, vUV.x * 3.0 + uTime * 4.0));
    float flicker = pulse * turb;

    // Atlas layout (shared with D3D9Client/D3D9Effect.cpp): the left
    // strip U < ~0.25 is the core plume gradient, the upper-right
    // quadrant U > ~0.5 is reserved for the flare/halo glow. Some
    // Exhaust.dds atlases don't actually ship a pre-baked halo in
    // that quadrant, so the flare branch synthesises the radial
    // falloff analytically from the atlas UVs rather than trusting
    // the texel alpha — any leftover texel that happens to be white
    // would otherwise show as a hard-edged rectangle around the
    // nozzle.
    vec3 col;
    float outAlpha;
    if (vUV.x > 0.4) {
        // Remap flare-quadrant UVs to a local 0..1 space, build a
        // soft radial mask (1 at centre, 0 at corners) and blend
        // toward the hot-core colour so the halo reads as the same
        // temperature as the plume it envelops.
        vec2 flareUV = (vUV - vec2(0.504, 0.004)) / vec2(0.492, 0.492);
        float r = length(flareUV - vec2(0.5)) * 2.0;        // 0..√2
        float mask = 1.0 - smoothstep(0.0, 1.0, r);         // cubic-like falloff
        vec3  inner = vec3(1.00, 0.96, 0.85);
        col = inner * mask * flicker;
        outAlpha = mask * uAlpha * flicker * 0.55;          // halo softer than core
    } else {
        // Inner (hot white) vs outer (warm yellow) core — U runs 0..1
        // along the plume axis in texture space, roughly 0=tail, 1=nozzle.
        vec3 inner = vec3(1.00, 0.96, 0.85);
        vec3 outer = vec3(1.00, 0.55, 0.18);
        float coreT = smoothstep(0.05, 0.24, vUV.x);
        vec3  ramp  = mix(outer, inner, coreT);

        // Soft V-edge falloff so the quad fades to zero at the flank
        // instead of clipping to rectangular silhouette.
        float vEdge = 1.0 - abs(vUV.y - 0.5) * 2.0;
        vEdge = smoothstep(0.0, 0.35, vEdge);

        col = tex.rgb * ramp * vEdge * flicker;
        outAlpha = tex.a * uAlpha * flicker;
    }

    // Additive blending upstream (GL_ONE, GL_ONE).
    FragColor = vec4(col * uAlpha, outAlpha);
}
