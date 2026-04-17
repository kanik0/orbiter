#version 410 core
in vec2 vUV;
uniform sampler2D uTexture;
uniform bool uUseColorKey;
uniform vec3 uColorKey;
uniform float uAlpha;
out vec4 FragColor;
void main() {
    vec4 color = texture(uTexture, vUV);
    if (uUseColorKey) {
        vec3 diff = abs(color.rgb - uColorKey);
        if (diff.r < 0.02 && diff.g < 0.02 && diff.b < 0.02)
            discard;
    }
    FragColor = vec4(color.rgb, color.a * uAlpha);
}
