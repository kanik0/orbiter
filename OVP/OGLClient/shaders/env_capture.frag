#version 410 core
#include "common.glsl"

// Analytic environment capture — one fragment per cube-map texel, reconstructs
// the world-space direction from face id + NDC, then returns a cheap sky
// model (space ambient + sun disc + soft glow). This feeds the prefilter
// pass below and is expensive enough that we only run it on init / on sun
// re-sync, not per frame.

in vec2 vNdc;

uniform int  uFace;          // 0 = +X … 5 = -Z, matches GL_TEXTURE_CUBE_MAP_*
uniform vec3 uSunDir;        // unit vector, world-space
uniform vec3 uSunColor;      // linear radiance
uniform vec3 uSpaceAmbient;  // dark space tint

out vec4 FragColor;

// Cube-face to world-direction mapping that matches the OpenGL convention for
// GL_TEXTURE_CUBE_MAP_POSITIVE_X etc. (the classical Renderbrary tables).
vec3 faceDir(int face, vec2 uv) {
    vec3 d;
    if      (face == 0) d = vec3( 1.0, -uv.y, -uv.x); // +X
    else if (face == 1) d = vec3(-1.0, -uv.y,  uv.x); // -X
    else if (face == 2) d = vec3( uv.x,  1.0,  uv.y); // +Y
    else if (face == 3) d = vec3( uv.x, -1.0, -uv.y); // -Y
    else if (face == 4) d = vec3( uv.x, -uv.y,  1.0); // +Z
    else                d = vec3(-uv.x, -uv.y, -1.0); // -Z
    return normalize(d);
}

void main() {
    vec3 dir = faceDir(uFace, vNdc);

    vec3 sky = uSpaceAmbient;

    // Sun disc: sharp core inside ~0.27° (solar radius as seen from Earth)
    // plus a softer halo that tapers over a few degrees so the prefilter
    // pass has something to average around the disc.
    float mu = dot(dir, uSunDir);
    float coreT = smoothstep(0.99995, 1.0, mu);
    float haloT = smoothstep(0.985,   1.0, mu);
    sky += uSunColor * coreT * 40.0;
    sky += uSunColor * haloT * 2.0;

    FragColor = vec4(sky, 1.0);
}
