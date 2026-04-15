#version 410 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
uniform mat4 uViewProj;
uniform mat4 uModel;
uniform float uInnerRad;
uniform float uOuterRad;
out vec2 vUV;
void main() {
    float radius = mix(uOuterRad, uInnerRad, aUV.x);
    vec3 scaledPos = aPos * radius;
    vec4 worldPos = uModel * vec4(scaledPos, 1.0);
    gl_Position = uViewProj * worldPos;
    vUV = aUV;
}
