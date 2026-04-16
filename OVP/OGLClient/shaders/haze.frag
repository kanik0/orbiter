#version 410 core
in float vAlpha;

uniform vec3 uHazeColor;
uniform float uHazeAlpha;

out vec4 FragColor;

void main() {
    float a = vAlpha * uHazeAlpha;
    FragColor = vec4(uHazeColor, a);
}
