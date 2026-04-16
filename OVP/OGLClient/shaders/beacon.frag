#version 410 core
in vec2 vUV;
uniform vec3 uColor;
uniform float uAlpha;
out vec4 FragColor;
void main() {
    // Soft circle falloff from center
    vec2 d = vUV - vec2(0.5);
    float r = length(d) * 2.0;
    float a = 1.0 - smoothstep(0.6, 1.0, r);
    FragColor = vec4(uColor * uAlpha, a * uAlpha);
}
