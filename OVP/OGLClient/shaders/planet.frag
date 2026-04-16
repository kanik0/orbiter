#version 410 core
in float vLight;
in vec3 vNormal;
uniform vec3 uColor;
out vec4 FragColor;
void main() {
    // Boost ambient so dark side is still visible
    float light = max(0.15, vLight);
    FragColor = vec4(uColor * light, 1.0);
}
