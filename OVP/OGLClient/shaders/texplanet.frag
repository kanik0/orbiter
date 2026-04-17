#version 410 core

in float vLight;
in float vNdotL;
in vec3  vNormal;
in vec2  vUV;

uniform sampler2D uTexture;

// Night lights overlay. When uHasNight is false the sampler is never touched
// (the GLSL compiler still requires it to be declared, but an unbound unit
// is safe with our "no-op" hasNight = 0 path).
uniform bool      uHasNight;
uniform sampler2D uNightTex;
uniform float     uNightIntensity;  // emissive scaling (typical 1.0-2.5)

out vec4 FragColor;

void main() {
    vec4 texColor = texture(uTexture, vUV);
    vec3 dayColor = texColor.rgb * vLight;

    if (uHasNight) {
        // Night lights fade in as the raw NdotL drops below 0 (past the
        // terminator). smoothstep(0, -0.12, NdotL) gives a ~7-degree soft
        // transition band, which matches the real civil-twilight zone and
        // hides the terminator seam tone-mapping tends to produce.
        float nightMix  = smoothstep(0.0, -0.12, vNdotL);
        vec3  nightRGB  = texture(uNightTex, vUV).rgb * uNightIntensity;
        dayColor += nightRGB * nightMix;
    }

    FragColor = vec4(dayColor, texColor.a);
}
