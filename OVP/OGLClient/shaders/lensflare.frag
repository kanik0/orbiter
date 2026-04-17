#version 410 core
// Lens flare overlay.
//
// Emits an additive RGB contribution centred on the projected sun position,
// built from: (1) a bright chromatic core, (2) soft halo, (3) a bright ring,
// (4) three reflected "ghosts" along the sun-to-centre axis with varying
// hue, (5) a thin anamorphic horizontal streak. The chromatic aberration
// on the core sells the lens as a real optical element instead of a
// perfectly-aligned ideal pinhole.

in vec2 vUV;

uniform vec2  uSunPos;       // sun position in UV [0,1]
uniform float uIntensity;
uniform vec2  uResolution;

out vec4 FragColor;

void main() {
    vec2 uv = vUV;
    vec2 sun = uSunPos;

    // Aspect-correct distance so "circular" ghosts stay circular on non-square viewports.
    vec2 toSun = (uv - sun) * vec2(uResolution.x / uResolution.y, 1.0);
    float d = length(toSun);

    // Direction toward the screen centre — reflected ghosts slide along this.
    vec2 toCentre = vec2(0.5) - sun;

    vec3 flare = vec3(0.0);

    // (1) Chromatic aberration on the central core — each channel has its
    // own radius falloff, so the edge fringes red and inner fringes cyan
    // exactly like a real achromatic lens.
    float rFall = exp(-d * 26.0);
    float gFall = exp(-d * 30.0);
    float bFall = exp(-d * 34.0);
    flare += vec3(rFall, gFall, bFall) * 0.9;

    // (2) Soft warm halo.
    flare += vec3(1.00, 0.95, 0.80) * exp(-d * 7.0) * 0.55;

    // (3) Thin cool ring.
    float ring = abs(d - 0.15);
    flare += vec3(0.45, 0.60, 1.00) * exp(-ring * 60.0) * 0.18;

    // (4) Reflected ghosts along the optical axis.
    for (int i = 1; i <= 3; i++) {
        float t = float(i) * 0.4;
        vec2  ghostPos = sun + toCentre * t;
        vec2  toGhost  = (uv - ghostPos) * vec2(uResolution.x / uResolution.y, 1.0);
        float dg       = length(toGhost);
        float size     = 0.08 + float(i) * 0.03;
        float bright   = 0.12 / float(i);
        vec3  ghostCol = mix(vec3(0.8, 0.9, 1.0), vec3(1.0, 0.7, 0.4), float(i) / 3.0);
        flare += ghostCol * exp(-dg / size * 4.0) * bright;
    }

    // (5) Anamorphic horizontal streak — tighter along y, wider along x.
    float streakY = exp(-abs(uv.y - sun.y) * uResolution.y * 0.02);
    float streakX = exp(-abs(uv.x - sun.x) * 4.0);
    flare += vec3(1.0, 0.9, 0.7) * streakY * streakX * 0.18;

    FragColor = vec4(flare * uIntensity, 1.0);
}
