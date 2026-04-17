// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// OGLParticle - Particle stream system (exhaust trails, reentry effects)

#ifndef __OGLPARTICLE_H
#define __OGLPARTICLE_H

#ifndef _WIN32
#include "GraphicsAPI.h"
#include <OpenGL/gl3.h>
#include <vector>

struct OGLTexture;

namespace ogl {

class ShaderMgr;

// A single particle in the stream.
//
// `streamAlpha` captures the stream level → alpha mapping evaluated at
// spawn time (see PARTICLESTREAMSPEC::LEVELMAP). It's kept constant over
// the particle's life; the per-frame opacity is streamAlpha * lifeCurve(t).
// `tint0` holds the colour at birth — for DIFFUSE streams we interpolate
// toward a desaturated grey over life to sell smoke dissipation.
struct Particle {
	VECTOR3 pos;         // global position
	VECTOR3 vel;         // velocity
	float   size;        // current size [m]
	float   streamAlpha; // envelope at spawn (0..1)
	float   alpha;       // current opacity after the life curve
	float   age;         // seconds since creation
	float   lifetime;    // max lifetime
	float   tint[3];     // birth colour — drifts toward grey for DIFFUSE
};

class OGLParticleStream : public oapi::ParticleStream {
public:
	OGLParticleStream(oapi::GraphicsClient *gc, PARTICLESTREAMSPEC *pss, ShaderMgr *shaderMgr);
	~OGLParticleStream();

	// Update particle positions and spawn new ones
	void Update(double simT, double dt);

	// Render all particles as billboards
	void Render(const float *vp, const VECTOR3 &camPos);

	// Static shared resources
	static void InitShared(ShaderMgr *shaderMgr, const std::string &texturePath);
	static void ReleaseShared();

private:
	ShaderMgr *m_shaderMgr;
	PARTICLESTREAMSPEC m_spec;
	std::vector<Particle> m_particles;
	double m_lastEmit;
	bool m_active;

	void EmitParticle(double simT);

	// Shared GL resources
	static GLuint s_shader;
	static GLuint s_vao, s_vbo;
	static OGLTexture *s_defaultTex;
	static bool s_initialized;
	static ShaderMgr *s_shaderMgr;
};

// Exhaust stream with vessel-specific rendering
class OGLExhaustStream : public OGLParticleStream {
public:
	OGLExhaustStream(oapi::GraphicsClient *gc, PARTICLESTREAMSPEC *pss, ShaderMgr *shaderMgr,
	                 OBJHANDLE hVessel, const double *lvl, const VECTOR3 *ref, const VECTOR3 *dir);
};

// Reentry stream with heating glow
class OGLReentryStream : public OGLParticleStream {
public:
	OGLReentryStream(oapi::GraphicsClient *gc, PARTICLESTREAMSPEC *pss, ShaderMgr *shaderMgr,
	                 OBJHANDLE hVessel);
};

} // namespace ogl

#endif // !_WIN32
#endif // __OGLPARTICLE_H
