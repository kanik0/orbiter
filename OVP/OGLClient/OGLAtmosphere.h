// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// OGLAtmosphere - Per-planet Rayleigh + Mie single-scattering.
//
// Each atmospheric planet owns one instance; OGLvPlanet calls Render() after
// the surface is drawn. The pass uses a fullscreen quad (no VBO — the vertex
// shader emits corners from gl_VertexID) and a custom shader that integrates
// scatter along the view ray for every pixel that maps into the atmosphere
// shell. Physics constants are seeded from the Orbiter ATMCONST and
// OBJPRM_PLANET_* parameters.

#ifndef __OGLATMOSPHERE_H
#define __OGLATMOSPHERE_H

#ifndef _WIN32
#include "OrbiterAPI.h"
#include <OpenGL/gl3.h>

namespace ogl {

class ShaderMgr;

class OGLAtmosphere {
public:
	OGLAtmosphere(OBJHANDLE hPlanet, ShaderMgr *shaderMgr);
	~OGLAtmosphere();

	bool HasAtmosphere() const { return m_hasAtmo; }

	// Render the atmosphere pass. `vp` is the render-time view-projection
	// matrix (its inverse is computed in the frag shader). `camPos` and
	// `planetPos` are in Orbiter's absolute-metres world frame;
	// `planetRadius` is the planet radius in metres.
	void Render(const float *vp,
	            const VECTOR3 &camPos, const VECTOR3 &sunPos,
	            double planetRadius, const VECTOR3 &planetPos);

private:
	OBJHANDLE m_hPlanet;
	ShaderMgr *m_shaderMgr;
	bool m_hasAtmo;

	// Atmosphere parameters (cached at construction time).
	double m_atmoAlt;        // altitude limit of the shell [m]
	double m_horizonAlt;     // legacy horizon haze altitude [m]
	VECTOR3 m_skyColor;      // ATMCONST sky colour at sea level
	// Single-scattering constants (populated per planet from ATMCONST).
	float m_scaleR;          // Rayleigh scale height [m]
	float m_scaleM;          // Mie scale height     [m]
	float m_betaR[3];        // Rayleigh extinction per wavelength [m^-1]
	float m_betaM;           // Mie extinction                     [m^-1]
	float m_mieG;            // Henyey-Greenstein asymmetry
	float m_sunRadiance[3];  // pre-exposure radiance (scaled by color0 tint)
	float m_exposure;        // atmosphere-only exposure

	// Fullscreen quad state. The vertex shader emits corners from gl_VertexID
	// so we only need an empty VAO to satisfy the driver.
	GLuint m_quadVAO;
	GLuint m_shader;
};

} // namespace ogl

#endif // !_WIN32
#endif // __OGLATMOSPHERE_H
