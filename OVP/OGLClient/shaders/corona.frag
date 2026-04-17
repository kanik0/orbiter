#version 410 core
#include "common.glsl"

// Solar corona + rays. Given the sun's projected NDC position and the
// viewport aspect, we paint a radial gradient with three components:
//   1) a bright core disc (inside ~0.4° apparent radius)
//   2) a soft chromatic halo fading over the next ~3°
//   3) six faint rays that rotate slowly with uTime
// Additive blending upstream (GL_ONE, GL_ONE) means we only need to
// emit the contribution; the scene underneath passes through unchanged
// where the corona is transparent.

in vec2 vNdc;

uniform vec2  uSunNdc;         // sun position in NDC [-1..1]
uniform vec2  uResolution;     // viewport size (px) for aspect correction
uniform float uTime;           // sim time (s) — drives ray rotation
uniform float uIntensity;      // master multiplier (0 = off)
uniform int   uSunVisible;     // 0 when the sun is behind the camera

out vec4 FragColor;

void main() {
    if (uSunVisible == 0 || uIntensity < 1e-4) discard;

    // Aspect-corrected distance so the halo stays circular on non-square
    // viewports.
    vec2  aspect = vec2(uResolution.x / uResolution.y, 1.0);
    vec2  delta  = (vNdc - uSunNdc) * aspect;
    float d      = length(delta);

    // Tight hot core (white-cream).
    float core = exp(-d * 180.0);
    vec3  coreCol = vec3(1.00, 0.96, 0.88);

    // Soft chromatic halo: inner yellow, outer orange-red.
    float halo   = exp(-d * 6.5);
    vec3  haloCol = mix(vec3(1.0, 0.55, 0.18), vec3(1.0, 0.92, 0.70),
                        clamp(1.0 - d * 2.5, 0.0, 1.0));

    // Six radial rays rotating slowly. The twist is tiny (0.03 rad/s) —
    // real solar rays don't spin, but a static ray pattern reads as a
    // rendering artefact; a slow drift sells the hot plasma idea.
    float ang      = atan(delta.y, delta.x);
    float rayAngle = ang + uTime * 0.03;
    float rayPow   = abs(cos(rayAngle * 3.0));   // 6 lobes
    rayPow         = pow(rayPow, 18.0);
    float rayFade  = exp(-d * 10.0);
    vec3  rayCol   = vec3(1.0, 0.88, 0.62);

    vec3 rgb = coreCol * core
             + haloCol * halo * 0.45
             + rayCol  * rayPow * rayFade * 0.35;

    FragColor = vec4(rgb * uIntensity, 1.0);
}
