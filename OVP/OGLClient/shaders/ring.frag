#version 410 core
in vec2 vUV;
uniform sampler2D uTexture;
out vec4 FragColor;
void main() {
    vec4 color = texture(uTexture, vUV);
    float alpha = color.r * 0.75;
    FragColor = vec4(color.rgb, alpha);
}
