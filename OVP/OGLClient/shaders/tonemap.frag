#version 410 core
#include "common.glsl"

// Final compositor: HDR scene + bloom → ACES filmic tone map → sRGB output.
// ACES is the industry-standard filmic curve used by virtually every modern
// PBR renderer; its shoulder rolls off smoothly at the sun disc without the
// grey milkiness Reinhard produces on very bright highlights.

in vec2 vUV;

uniform sampler2D uScene;
uniform sampler2D uBloom;
uniform float     uExposure;
uniform float     uBloomIntensity;

out vec4 FragColor;

// Stephen Hill's ACES fit (2016) — the community-standard analytical
// approximation of the full ACES reference transform.
vec3 RRTAndODTFit(vec3 v) {
    vec3 a = v * (v + 0.0245786) - 0.000090537;
    vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
    return a / b;
}

vec3 tonemapACES(vec3 hdr) {
    // Input transform (linear-sRGB → ACES cg).
    const mat3 ACESInputMat = mat3(
        0.59719, 0.07600, 0.02840,
        0.35458, 0.90834, 0.13383,
        0.04823, 0.01566, 0.83777
    );
    const mat3 ACESOutputMat = mat3(
         1.60475, -0.10208, -0.00327,
        -0.53108,  1.10813, -0.07276,
        -0.07367, -0.00605,  1.07602
    );
    vec3 v = ACESInputMat * hdr;
    v = RRTAndODTFit(v);
    return clamp(ACESOutputMat * v, 0.0, 1.0);
}

// uIsHDR=1 applies the full HDR ACES pipeline; 0 skips ACES and simply
// gamma-corrects the already-LDR scene. The macOS OpenGL 4.1-via-Metal
// backend can't write to GL_RGBA16F colour attachments reliably, so the
// scene FBO is forced to GL_RGBA8 (linear) and the shader must not push
// the small input values through the ACES RRT/ODT curve — its output
// matrix has negative coefficients that clamp dark pixels to pure zero.
uniform int uIsHDR;

void main() {
    vec3 scene = texture(uScene, vUV).rgb;
    if (uBloomIntensity > 0.0) {
        scene += texture(uBloom, vUV).rgb * uBloomIntensity;
    }

    vec3 mapped;
    if (uIsHDR != 0) {
        // HDR path: ACES filmic tone map preserves highlight roll-off
        // for sun discs with values >> 1.0.
        scene *= uExposure;
        mapped = tonemapACES(scene);
    } else {
        // LDR path (macOS default — RGBA8 scene FBO). uExposure now lands
        // through correctly after the ShaderMgr cache-collision fix (#49)
        // that used to hand out stale uniform locations after bloom / lens
        // flare programs ran. The previous workaround (hardcoded 8× gain)
        // over-exposed every textured planet to pure white (#25); now the
        // caller's value drives the mapping and textured bodies retain
        // their albedo.
        scene *= uExposure;
        mapped = clamp(scene, 0.0, 1.0);
    }

    // sRGB encoding via gamma 2.2. Input is assumed linear; output
    // matches what the backbuffer expects for sRGB-display-mapped pixels.
    mapped = pow(mapped, vec3(1.0 / 2.2));

    FragColor = vec4(mapped, 1.0);
}
