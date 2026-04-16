#version 410 core
in vec2 vUV;
uniform sampler2D uScene;
uniform float uThreshold;
out vec4 FragColor;

void main() {
    vec3 color = texture(uScene, vUV).rgb;
    // Extract pixels brighter than threshold
    float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));
    if (brightness > uThreshold)
        FragColor = vec4(color * (brightness - uThreshold), 1.0);
    else
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
}
