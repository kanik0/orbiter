#version 410 core

in float vBrightness;
in vec3  vColor;

out vec4 FragColor;

void main() {
    // gl_PointCoord runs [0..1] across the sprite quad; centre is (0.5, 0.5).
    // A soft circular falloff turns the default square point into a round
    // star and hides the pixel grid at the sprite edge.
    vec2  d  = gl_PointCoord - vec2(0.5);
    float r2 = dot(d, d) * 4.0;   // 0 at centre, 1 at radius 0.5
    if (r2 > 1.0) discard;

    float falloff = exp(-r2 * 3.5);

    // Additive blending upstream means alpha does not matter beyond 0/1;
    // we emit a slightly oversaturated core so bright stars punch through
    // the ACES tonemap in M11.
    vec3 rgb = vColor * (vBrightness * 1.2) * falloff;
    FragColor = vec4(rgb, falloff);
}
