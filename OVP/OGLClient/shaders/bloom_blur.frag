#version 410 core
in vec2 vUV;
uniform sampler2D uImage;
uniform bool uHorizontal;
out vec4 FragColor;

// 9-tap Gaussian blur weights
const float weight[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main() {
    vec2 texOffset = 1.0 / textureSize(uImage, 0); // pixel size
    vec3 result = texture(uImage, vUV).rgb * weight[0];

    if (uHorizontal) {
        for (int i = 1; i < 5; i++) {
            result += texture(uImage, vUV + vec2(texOffset.x * i, 0.0)).rgb * weight[i];
            result += texture(uImage, vUV - vec2(texOffset.x * i, 0.0)).rgb * weight[i];
        }
    } else {
        for (int i = 1; i < 5; i++) {
            result += texture(uImage, vUV + vec2(0.0, texOffset.y * i)).rgb * weight[i];
            result += texture(uImage, vUV - vec2(0.0, texOffset.y * i)).rgb * weight[i];
        }
    }
    FragColor = vec4(result, 1.0);
}
