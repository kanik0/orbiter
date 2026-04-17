#version 410 core
#include "common.glsl"

// Cloud shell sampling with Lambert sun shading and a Fresnel horizon fade
// that eliminates the visible seam where the cloud sphere meets the planet's
// limb. Alpha comes from the texture (clouds occupy the RGB channels; their
// spatial extent lives in the luminance).

in vec3 vNormal;
in vec2 vUV;
in vec3 vViewDir;
in float vSunDot;

uniform sampler2D uCloudTex;
uniform vec3 uTint;        // per-planet cloud tint (Earth whites, Venus yellows)
uniform float uOpacity;    // global cloud layer opacity [0..1]
uniform float uAmbient;    // ambient floor on the night side

out vec4 FragColor;

void main() {
    vec4 tex = texture(uCloudTex, vUV);

    // Use luminance as alpha so sky-blue cloud textures still behave as
    // semi-transparent spots over open sky.
    float cloudMask = max(max(tex.r, tex.g), tex.b);
    float alpha     = cloudMask * uOpacity;
    if (alpha < 0.01) discard;

    // Sun lighting — clouds scatter strongly so keep a bright ambient floor.
    float lit = max(uAmbient, vSunDot);

    // Fresnel horizon fade: at grazing angles the cloud sheet would appear
    // solid which breaks the silhouette. Fading the alpha as the normal
    // approaches 90° from the view direction preserves the limb softness
    // that horizon haze used to cover.
    vec3  N = normalize(vNormal);
    vec3  V = normalize(vViewDir);
    float ndotv = clamp(dot(N, V), 0.0, 1.0);
    float fresnelFade = smoothstep(0.05, 0.35, ndotv);

    vec3  cloudColor = tex.rgb * uTint * lit;
    FragColor = vec4(cloudColor, alpha * fresnelFade);
}
