#version 410 core
in vec2 vUV;
uniform sampler2D uTexture;
uniform float uAlpha;
out vec4 FragColor;
void main() {
    vec4 color = texture(uTexture, vUV);
    FragColor = vec4(color.rgb * uAlpha, color.a * uAlpha);
}
