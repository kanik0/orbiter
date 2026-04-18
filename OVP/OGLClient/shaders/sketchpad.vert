#version 410 core

// 2D drawing for oapi::Sketchpad. Input is pixel coords in the bound
// render-target's space (top-left origin); UV is only read when the
// fragment shader samples a texture (text/blit modes).
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;

uniform vec2 uViewport;       // target width/height in pixels
uniform mat4 uWorld;          // SetWorldTransform (identity by default)
uniform mat4 uViewProj;       // active VP matrix when uViewMode == 1
uniform int  uViewMode;       // 0 = pixel-space ORTHO, 1 = user VP

out vec2  vUV;
out vec4  vWorldPos;          // passthrough for Clipper in fragment stage

void main() {
    vec4 wp = uWorld * vec4(aPos, 0.0, 1.0);
    vWorldPos = wp;

    if (uViewMode == 1) {
        gl_Position = uViewProj * wp;
    } else {
        vec2 ndc = vec2(
            wp.x / uViewport.x * 2.0 - 1.0,
            1.0 - wp.y / uViewport.y * 2.0
        );
        gl_Position = vec4(ndc, 0.0, 1.0);
    }
    vUV = aUV;
}
