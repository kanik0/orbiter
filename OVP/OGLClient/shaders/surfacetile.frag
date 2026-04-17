#version 410 core
in float vLight;
in vec3 vNormal;
in vec2 vUV;
in vec3 vWorldPos;

uniform sampler2D uTexture;
uniform bool uHasTexture;

out vec4 FragColor;

void main() {
    vec3 color;
    if (uHasTexture) {
        color = texture(uTexture, vUV).rgb;
    } else {
        // Fallback: gray surface
        color = vec3(0.5, 0.5, 0.5);
    }
    FragColor = vec4(color * vLight, 1.0);
}
