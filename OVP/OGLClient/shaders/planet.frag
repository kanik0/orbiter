#version 410 core
in float vLight;
in vec3 vNormal;
uniform vec3 uColor;
out vec4 FragColor;
void main() {
    float light = max(0.25, vLight);
    FragColor = vec4(uColor * light, 1.0);
}
