#version 410 core
in float vBrightness;
in vec3 vColor;
out vec4 FragColor;
void main() {
    FragColor = vec4(vColor * vBrightness, 1.0);
}
