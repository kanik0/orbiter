#version 410 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
uniform mat4 uViewProj;
uniform mat4 uModel;
uniform vec3 uSunDir;
out float vLight;
out vec3 vNormal;
out vec2 vUV;
void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    gl_Position = uViewProj * worldPos;
    vec3 worldNormal = normalize(mat3(uModel) * aNormal);
    vLight = max(0.15, dot(worldNormal, uSunDir));
    vNormal = worldNormal;
    vUV = aUV;
}
