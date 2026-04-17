// Shadow mapping utilities shared between vessel shaders.

#pragma once

// Project a world-space position into the light's clip-space-to-NDC-to-[0,1]
// shadow map sampling frame.
vec3 worldToShadow(mat4 lightVP, vec3 worldPos) {
    vec4 lsPos = lightVP * vec4(worldPos, 1.0);
    vec3 proj  = lsPos.xyz / lsPos.w;
    return proj * 0.5 + 0.5;
}

// PCF 3x3 sampling. `bias` is the depth offset used to suppress shadow acne;
// the caller picks a value proportional to the sun angle (slope-scale bias
// is applied on the shadow_cast side via polygon offset).
// Returns 1.0 = fully lit, 0.0 = fully shadowed.
float shadowPCF3x3(sampler2D shadowMap, vec3 shadowUV, float bias) {
    // Outside the shadow frustum → assume lit (the main light integral
    // handles that region via its own NdotL clamp).
    if (shadowUV.x < 0.0 || shadowUV.x > 1.0 ||
        shadowUV.y < 0.0 || shadowUV.y > 1.0 ||
        shadowUV.z > 1.0)
        return 1.0;

    vec2  texel  = 1.0 / vec2(textureSize(shadowMap, 0));
    float refD   = shadowUV.z - bias;
    float shadow = 0.0;
    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            float d = texture(shadowMap, shadowUV.xy + vec2(x, y) * texel).r;
            shadow += (refD > d) ? 0.0 : 1.0;
        }
    }
    return shadow / 9.0;
}

// Compute a slope-scaled depth bias: steeper sun-to-normal angles need more
// bias to keep sun-lit pixels self-shadow-free without producing Peter Pan.
float shadowBias(float NdotL) {
    return clamp(0.005 * (1.0 - NdotL), 0.0005, 0.01);
}
