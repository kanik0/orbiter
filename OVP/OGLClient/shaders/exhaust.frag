#version 410 core
// Exhaust plume billboard. The texture atlas packs the plume gradient along
// U (u≈0.24 = nozzle root = hot core; u≈0.01 = far tail = cool fade). We
// mix a warm inner tone over a hotter core colour based on U so the plume
// renders as a genuine bicolor jet instead of a flat orange stripe, and
// soften the V edges so the quad silhouette doesn't read as a hard box.

in vec2 vUV;

uniform sampler2D uTexture;
uniform float     uAlpha;

out vec4 FragColor;

void main() {
    vec4 tex = texture(uTexture, vUV);

    // Inner (hot white) vs outer (warm yellow) core — U runs 0..1 along
    // the plume axis in texture space, roughly 0=tail, 1=nozzle.
    vec3 inner = vec3(1.00, 0.96, 0.85);
    vec3 outer = vec3(1.00, 0.55, 0.18);
    float coreT = smoothstep(0.05, 0.24, vUV.x);
    vec3  ramp  = mix(outer, inner, coreT);

    // Soft V-edge falloff so the quad fades to zero at the flank instead
    // of clipping to rectangular silhouette.
    float vEdge = 1.0 - abs(vUV.y - 0.5) * 2.0;
    vEdge = smoothstep(0.0, 0.35, vEdge);

    vec3 col = tex.rgb * ramp * vEdge;
    // Additive blending upstream (GL_ONE, GL_ONE).
    FragColor = vec4(col * uAlpha, tex.a * uAlpha);
}
