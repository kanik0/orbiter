// gamma.glsl.inc — sRGB / linear conversions.
//
// Orbiter's asset pipeline stores most textures in sRGB; shaders want to
// work in linear space and tonemap back at the end. Use these helpers
// instead of hard-coding `pow(c, 2.2)` everywhere.

#pragma once

vec3 srgbToLinear(vec3 c) {
    // IEC 61966-2-1 curve approximation; cheap and accurate enough for
    // realtime PBR — the piecewise form only matters for dark values.
    return pow(max(c, vec3(0.0)), vec3(2.2));
}

vec3 linearToSrgb(vec3 c) {
    return pow(max(c, vec3(0.0)), vec3(1.0 / 2.2));
}

vec4 srgbToLinear(vec4 c) { return vec4(srgbToLinear(c.rgb), c.a); }
vec4 linearToSrgb(vec4 c) { return vec4(linearToSrgb(c.rgb), c.a); }
