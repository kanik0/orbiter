#version 410 core
// Fullscreen triangle (no VBO required: emitted from gl_VertexID).
// Pass the NDC xy through so the fragment shader can reconstruct the ray.

out vec2 vNdc;

void main() {
    // Six-vertex strip covering the entire NDC square.
    const vec2 corners[4] = vec2[4](
        vec2(-1.0, -1.0),
        vec2( 1.0, -1.0),
        vec2(-1.0,  1.0),
        vec2( 1.0,  1.0)
    );
    vec2 p = corners[gl_VertexID];
    vNdc = p;
    gl_Position = vec4(p, 1.0, 1.0);  // z=1 → far plane; depth test reads > 0
}
