// ibl.glsl.inc — Image-based lighting helpers.
//
// Placeholder of the IBL sampling API that M9 (environment maps) will
// implement with prefiltered cubemaps. For now we only provide the
// single-LOD sampler already used by vessel_pbr.frag so existing shaders
// can migrate without behaviour changes.

#pragma once

vec3 iblDiffuse(samplerCube envMap, vec3 N) {
    // M9: replace with diffuse irradiance cubemap lookup.
    // Until then we fall back to the top-mip coarse colour, which matches
    // the pre-M9 behaviour of vessel_pbr.frag.
    return textureLod(envMap, N, 6.0).rgb;
}

vec3 iblSpecular(samplerCube envMap, vec3 R, float roughness) {
    float mipLevel = roughness * 6.0;
    return textureLod(envMap, R, mipLevel).rgb;
}
