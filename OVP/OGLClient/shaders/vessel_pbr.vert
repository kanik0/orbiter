#version 410 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

uniform mat4 uViewProj;
uniform mat4 uModel;
uniform vec3 uSunDir;
uniform vec3 uCamPos;       // camera position (camera-relative world space)

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUV;
out vec3 vSunDir;
out vec3 vViewDir;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    gl_Position = uViewProj * worldPos;

    vWorldPos = worldPos.xyz;
    vNormal = normalize(mat3(uModel) * aNormal);
    vUV = aUV;
    vSunDir = uSunDir;
    vViewDir = normalize(-worldPos.xyz); // camera at origin in camera-relative space
}
