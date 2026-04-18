#version 410 core

// Three render modes share one program so Sketchpad never has to swap
// programs in the middle of a frame:
//   0 = solid colour fill (Pen/Brush primitives)
//   1 = textured modulate (CopyRect/StretchRect family)
//   2 = alpha-mask text  (ImGui font atlas is single-channel alpha)
in vec2 vUV;

uniform vec4 uColor;
uniform int  uMode;
uniform sampler2D uTexture;

out vec4 FragColor;

void main() {
    if (uMode == 0) {
        FragColor = uColor;
    } else if (uMode == 1) {
        FragColor = texture(uTexture, vUV) * uColor;
    } else {
        // ImGui atlas: the glyph alpha sits in the r channel for the
        // alpha-8 path and in a (as RGBA32) for the RGBA path. Sampling
        // .a gives us the right mask in both cases.
        float a = texture(uTexture, vUV).a;
        FragColor = vec4(uColor.rgb, uColor.a * a);
    }
}
