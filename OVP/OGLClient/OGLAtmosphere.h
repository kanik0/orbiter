// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// OGLAtmosphere - Atmospheric rendering (horizon haze, sky dome, fog)

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

	// Check if this planet has an atmosphere
	bool HasAtmosphere() const { return m_hasAtmo; }

	// Render the horizon haze ring and sky dome
	void Render(const float *vp, const VECTOR3 &camPos, const VECTOR3 &sunPos,
	            double planetRadius, const VECTOR3 &planetPos);

private:
	OBJHANDLE m_hPlanet;
	ShaderMgr *m_shaderMgr;
	bool m_hasAtmo;

	// Atmosphere parameters (from Orbiter API)
	double m_atmoAlt;      // atmosphere altitude limit [m]
	double m_horizonAlt;   // horizon rendering altitude [m]
	VECTOR3 m_skyColor;    // daytime sky color at sea level
	double m_hazeDensity;  // haze opacity factor
	double m_hazeExtent;   // inner radius of haze ring (0-1)

	// Haze ring geometry
	GLuint m_hazeVAO, m_hazeVBO, m_hazeEBO;
	int m_hazeIndexCount;
	GLuint m_hazeShader;

	void InitHazeRing();
	void ReleaseHazeRing();
};

} // namespace ogl

#endif // !_WIN32
#endif // __OGLATMOSPHERE_H
