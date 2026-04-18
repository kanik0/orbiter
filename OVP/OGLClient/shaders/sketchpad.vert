#version 410 core

// 2D drawing for oapi::Sketchpad. Input is pixel coords in the bound
// render-target's space (top-left origin); UV is only read when the
// fragment shader samples a texture (text/blit modes).
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;

uniform vec2 uViewport;   // target width/height in pixels

out vec2 vUV;

void main() {
    vec2 ndc = vec2(
        aPos.x / uViewport.x * 2.0 - 1.0,
        1.0 - aPos.y / uViewport.y * 2.0
    );
    gl_Position = vec4(ndc, 0.0, 1.0);
    vUV = aUV;
}
