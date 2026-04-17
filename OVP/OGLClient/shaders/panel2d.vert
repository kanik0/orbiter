#version 410 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
uniform mat3 uTransform;
uniform vec2 uViewport;
out vec2 vUV;
void main() {
    vec3 transformed = uTransform * vec3(aPos, 1.0);
    // Convert from pixel coords to NDC: x: [0,w]->[−1,1], y: [0,h]->[1,−1]
    vec2 ndc = vec2(
        transformed.x / uViewport.x * 2.0 - 1.0,
        1.0 - transformed.y / uViewport.y * 2.0
    );
    gl_Position = vec4(ndc, 0.0, 1.0);
    vUV = aUV;
}
