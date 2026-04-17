#version 410 core
// Depth-only vertex program for the shadow pre-pass. Runs over the same
// VAO layout the main vessel shader uses (location 0 = position, 1/2/3 are
// present but ignored). Orthographic light-space projection provided by
// the caller as uLightVP.

layout(location = 0) in vec3 aPos;
// keep the extra locations declared so the VAO from OGLvVessel binds cleanly
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec4 aTangent;

uniform mat4 uLightVP;
uniform mat4 uModel;

void main() {
    gl_Position = uLightVP * uModel * vec4(aPos, 1.0);
}
