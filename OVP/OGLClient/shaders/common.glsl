// Common lighting functions shared across shaders

// Compute basic sun illumination: diffuse Lambert with ambient floor
float sunLight(vec3 worldNormal, vec3 sunDir, float ambient) {
    return max(ambient, dot(worldNormal, sunDir));
}
