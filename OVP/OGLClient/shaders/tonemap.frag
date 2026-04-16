#version 410 core
in vec2 vUV;
uniform sampler2D uScene;
uniform sampler2D uBloom;
uniform float uExposure;
uniform float uBloomIntensity;
out vec4 FragColor;

void main() {
    vec3 hdrColor = texture(uScene, vUV).rgb;
    vec3 bloomColor = texture(uBloom, vUV).rgb;

    // Add bloom
    hdrColor += bloomColor * uBloomIntensity;

    // Exposure tone mapping
    vec3 mapped = vec3(1.0) - exp(-hdrColor * uExposure);

    // Gamma correction
    mapped = pow(mapped, vec3(1.0 / 2.2));

    FragColor = vec4(mapped, 1.0);
}
