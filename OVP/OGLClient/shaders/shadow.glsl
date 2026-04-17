// Shadow mapping utilities

// PCF shadow sampling (2x2 kernel)
float shadowPCF(sampler2D shadowMap, vec3 projCoords, float bias) {
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            float depth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += projCoords.z - bias > depth ? 1.0 : 0.0;
        }
    }
    return shadow / 9.0;
}

// Compute shadow coordinates from light-space matrix
vec3 computeShadowCoords(mat4 lightSpaceMatrix, vec3 worldPos) {
    vec4 lsPos = lightSpaceMatrix * vec4(worldPos, 1.0);
    vec3 projCoords = lsPos.xyz / lsPos.w;
    projCoords = projCoords * 0.5 + 0.5; // transform to [0,1]
    return projCoords;
}
