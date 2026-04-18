#version 410 core

// Three render modes share one program so Sketchpad never has to swap
// programs in the middle of a frame:
//   0 = solid colour fill (Pen/Brush primitives)
//   1 = textured modulate (CopyRect/StretchRect family)
//   2 = alpha-mask text  (ImGui font atlas is single-channel alpha)
in vec2 vUV;

uniform vec4 uColor;
uniform int  uMode;
uniform sampler2D uTexture;

// Per-draw colour transforms driven by SetBrightness / SetColorMatrix /
// SetRenderParam. Defaults are identity so calling code that never
// touches them pays zero visual cost.
uniform vec4 uBrightness;  // identity = (1,1,1,1)
uniform mat4 uColorMat;    // identity
uniform vec4 uGamma;       // rgb = 1/gamma exponent; identity = (1,1,1,_)
uniform vec4 uNoise;       // rgb = noise tint, a = blend factor (0 = off)

out vec4 FragColor;

// Hash-based pseudo-random noise; deterministic in gl_FragCoord so the
// same frame renders identically across redraws, which is what the D3D9
// reference does (see SKP3_PRM_NOISE behaviour in Glare.hlsl).
float pseudoNoise(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    vec4 c;
    if (uMode == 0) {
        c = uColor;
    } else if (uMode == 1) {
        c = texture(uTexture, vUV) * uColor;
    } else {
        // ImGui atlas: the glyph alpha sits in the r channel for the
        // alpha-8 path and in a (as RGBA32) for the RGBA path. Sampling
        // .a gives us the right mask in both cases.
        float a = texture(uTexture, vUV).a;
        c = vec4(uColor.rgb, uColor.a * a);
    }

    c *= uBrightness;
    c = uColorMat * c;
    c.rgb = pow(max(c.rgb, vec3(0.0)), uGamma.rgb);

    if (uNoise.a > 0.0) {
        float n = pseudoNoise(gl_FragCoord.xy);
        c.rgb = mix(c.rgb, uNoise.rgb, uNoise.a * n);
    }

    FragColor = c;
}
