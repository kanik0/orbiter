#version 410 core
in float vLight;
in vec3 vNormal;
uniform vec3 uColor;
out vec4 FragColor;
void main() {
    FragColor = vec4(uColor * vLight, 1.0);
}
