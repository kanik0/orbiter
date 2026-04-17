#version 410 core
// Grid line fragment: premultiplied-alpha additive tint. uAlpha modulates
// the full grid globally so the overlay can be faded up on toggle.

in vec3 vColor;

uniform float uAlpha;

out vec4 FragColor;

void main() {
    FragColor = vec4(vColor * uAlpha, uAlpha);
}
