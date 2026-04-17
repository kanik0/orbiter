// scatter_common.glsl.inc — Rayleigh/Mie scattering constants.
//
// Filled in by M4 (Atmosphere Rayleigh + Mie scattering). Kept as a stub
// now so the include path is reserved and the shader library compiles.

#pragma once

// Scale heights (metres). Populated from Scatter UBO in M4.
#define SCATTER_RAYLEIGH_H 8000.0
#define SCATTER_MIE_H      1200.0

// Phase function g-factor for Mie scattering.
#define SCATTER_MIE_G      0.76
