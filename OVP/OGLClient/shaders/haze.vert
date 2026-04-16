#version 410 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in float aAlpha;

uniform mat4 uViewProj;
uniform mat4 uModel;

out float vAlpha;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    gl_Position = uViewProj * worldPos;
    vAlpha = aAlpha;
}
