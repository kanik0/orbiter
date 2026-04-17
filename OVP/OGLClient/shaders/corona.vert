#version 410 core
// Solar corona billboard. The celsphere render frame already carries only
// camera rotation (no translation), so a unit-scale quad at the sun's
// projected direction renders as a fixed-screen-space halo — exactly the
// behaviour we want for a visual halo that doesn't parallax with motion.
//
// Fullscreen triangle strip emitted from gl_VertexID; fragment shader uses
// vNdc to compute the distance to the sun in screen space.

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
    gl_Position = vec4(p, 1.0, 1.0);  // z=1 → far plane; behind planets
}
