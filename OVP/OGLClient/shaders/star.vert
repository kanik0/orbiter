#version 410 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in float aBrightness;
layout(location = 2) in vec3 aColor;
out float vBrightness;
out vec3 vColor;
uniform mat4 uViewProj;
void main() {
    gl_Position = uViewProj * vec4(aPos, 1.0);
    gl_PointSize = max(1.0, aBrightness * 4.0);
    vBrightness = aBrightness;
    vColor = aColor;
}
