#version 410 core
in vec2 vUV;
uniform vec2 uSunPos;      // sun position in UV space [0,1]
uniform float uIntensity;
uniform vec2 uResolution;
out vec4 FragColor;

void main() {
    vec2 uv = vUV;
    vec2 sunUV = uSunPos;

    // Direction from sun to center
    vec2 toCenter = vec2(0.5) - sunUV;

    // Multiple ghost artifacts along the sun-center axis
    vec3 flare = vec3(0.0);

    // Ghost 1: large soft glow at sun position
    float d1 = length(uv - sunUV);
    flare += vec3(1.0, 0.95, 0.8) * exp(-d1 * 8.0) * 0.6;

    // Ghost 2: smaller bright core
    flare += vec3(1.0, 1.0, 0.9) * exp(-d1 * 30.0) * 0.8;

    // Ghost 3: ring artifact
    float ring = abs(d1 - 0.15);
    flare += vec3(0.5, 0.6, 1.0) * exp(-ring * 60.0) * 0.15;

    // Ghost 4-6: reflected ghosts along sun-center axis
    for (int i = 1; i <= 3; i++) {
        float t = float(i) * 0.4;
        vec2 ghostPos = sunUV + toCenter * t;
        float dg = length(uv - ghostPos);
        float ghostSize = 0.08 + float(i) * 0.03;
        float ghostBright = 0.1 / float(i);
        vec3 ghostCol = mix(vec3(0.8, 0.9, 1.0), vec3(1.0, 0.7, 0.4), float(i) / 3.0);
        flare += ghostCol * exp(-dg / ghostSize * 4.0) * ghostBright;
    }

    // Radial streaks (anamorphic)
    float streakH = exp(-abs(uv.y - sunUV.y) * uResolution.y * 0.02);
    float streakDist = abs(uv.x - sunUV.x);
    streakH *= exp(-streakDist * 4.0);
    flare += vec3(1.0, 0.9, 0.7) * streakH * 0.15;

    FragColor = vec4(flare * uIntensity, 1.0);
}
