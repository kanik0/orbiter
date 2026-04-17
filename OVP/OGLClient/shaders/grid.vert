#version 410 core
// Planetarium line renderer — shared by the equatorial and ecliptic grids.
// Runs under the celsphere's rotation-only VP so the grid sits at infinite
// distance and doesn't parallax with camera motion.

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;

uniform mat4 uViewProj;

out vec3 vColor;

void main() {
    gl_Position = uViewProj * vec4(aPos, 1.0);
    vColor = aColor;
}
