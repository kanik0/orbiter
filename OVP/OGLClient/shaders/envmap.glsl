// Environment map sampling utilities

// Sample environment cubemap with roughness-based LOD
vec3 sampleEnvMap(samplerCube envMap, vec3 R, float roughness) {
    float mipLevel = roughness * 6.0;
    return textureLod(envMap, R, mipLevel).rgb;
}

// Fresnel-modulated environment reflection
vec3 envReflection(samplerCube envMap, vec3 N, vec3 V, vec3 F0, float roughness) {
    vec3 R = reflect(-V, N);
    vec3 envColor = sampleEnvMap(envMap, R, roughness);
    float NdotV = max(dot(N, V), 0.001);
    vec3 F = F0 + (1.0 - F0) * pow(clamp(1.0 - NdotV, 0.0, 1.0), 5.0);
    return envColor * F * (1.0 - roughness * 0.5);
}
