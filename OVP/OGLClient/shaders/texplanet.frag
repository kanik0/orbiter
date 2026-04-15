#version 410 core
in float vLight;
in vec3 vNormal;
in vec2 vUV;
uniform sampler2D uTexture;
out vec4 FragColor;
void main() {
    vec4 texColor = texture(uTexture, vUV);
    FragColor = vec4(texColor.rgb * vLight, texColor.a);
}
