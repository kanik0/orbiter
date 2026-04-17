#version 410 core
// Fullscreen triangle (two triangles via TRIANGLE_STRIP) emitted from
// gl_VertexID — the capture / prefilter passes run one draw call per cube
// face and the fragment shader derives the direction vector from vNdc + the
// uFace uniform.

out vec2 vNdc;

void main() {
    const vec2 corners[4] = vec2[4](
        vec2(-1.0, -1.0),
        vec2( 1.0, -1.0),
        vec2(-1.0,  1.0),
        vec2( 1.0,  1.0)
    );
    vec2 p = corners[gl_VertexID];
    vNdc = p;
    gl_Position = vec4(p, 0.0, 1.0);
}
