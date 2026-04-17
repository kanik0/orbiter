#version 410 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec4 aTangent;  // xyz: tangent, w: bitangent handedness

uniform mat4 uViewProj;
uniform mat4 uModel;
uniform vec3 uSunDir;

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUV;
out vec3 vSunDir;
out vec3 vViewDir;
out vec3 vTangent;
out vec3 vBitangent;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    gl_Position = uViewProj * worldPos;

    mat3 nrmM   = mat3(uModel);
    vec3 N      = normalize(nrmM * aNormal);
    vec3 T      = normalize(nrmM * aTangent.xyz);
    // Re-orthogonalise so any model non-uniform-scale noise doesn't produce
    // a skewed TBN at the fragment.
    T           = normalize(T - N * dot(N, T));
    vec3 B      = cross(N, T) * aTangent.w;

    vWorldPos  = worldPos.xyz;
    vNormal    = N;
    vTangent   = T;
    vBitangent = B;
    vUV        = aUV;
    vSunDir    = uSunDir;
    vViewDir   = normalize(-worldPos.xyz);  // camera at origin in render frame
}
