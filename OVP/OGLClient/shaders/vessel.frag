#version 410 core
in float vLight;
in vec3 vNormal;
in vec2 vUV;
uniform vec4 uDiffuse;
uniform vec3 uEmissive;
uniform bool uHasTexture;
uniform sampler2D uTexture;
out vec4 FragColor;
void main() {
    vec4 baseColor = uDiffuse;
    if (uHasTexture) {
        vec4 texColor = texture(uTexture, vUV);
        baseColor *= texColor;
    }
    vec3 lit = baseColor.rgb * vLight + uEmissive;
    FragColor = vec4(lit, baseColor.a);
}
