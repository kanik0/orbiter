#version 410 core
// Particle billboard fragment. uTint carries the per-particle colour ramp
// (emissive streams start white-warm and hold, diffuse streams drift to
// grey over life so smoke dissipation reads correctly against dark space
// and daylit atmosphere alike).

in vec2 vUV;

uniform sampler2D uTexture;
uniform float     uAlpha;
uniform vec3      uTint;

out vec4 FragColor;

void main() {
    vec4 tex = texture(uTexture, vUV);
    vec3 col = tex.rgb * uTint;
    // Premultiplied-style alpha: the blend func already matches
    // (GL_ONE, GL_ONE_MINUS_SRC_ALPHA) for DIFFUSE and (GL_ONE, GL_ONE)
    // for EMISSIVE — outputting tinted colour directly is correct in
    // both branches.
    FragColor = vec4(col * uAlpha, tex.a * uAlpha);
}
