// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef _WIN32

#include "OGLvBase.h"
#include "OGLShaderMgr.h"
#include "OGLBeaconArray.h"

#include <cmath>
#include <cstdio>

namespace ogl {

OGLvBase::OGLvBase(OBJHANDLE hBase, ShaderMgr *shaderMgr)
	: OGLvObject(hBase, shaderMgr), m_built(false)
{
}

OGLvBase::~OGLvBase() {}

void OGLvBase::BuildLights()
{
	m_built = true;

	OBJHANDLE hPlanet = oapiGetBasePlanet(m_hObj);
	if (!hPlanet) return;

	const DWORD nPad = oapiGetBasePadCount(m_hObj);
	if (nPad == 0) return;

	char name[64] = {0};
	oapiGetObjectName(m_hObj, name, 64);

	// Two-colour PAPI-style grouping: every other pad flips its emitter so
	// adjacent beacons read as distinct sources. Real Orbiter bases don't
	// expose PAPI geometry; this is a reasonable approximation until M14.c
	// bolts on full runway parsing.
	for (DWORD i = 0; i < nPad; i++) {
		double lng, lat, rad;
		if (!oapiGetBasePadEquPos(m_hObj, i, &lng, &lat, &rad)) continue;

		VECTOR3 worldPos;
		oapiEquToGlobal(hPlanet, lng, lat, rad, &worldPos);

		PadLight p;
		p.pos  = worldPos;
		p.size = 60.0f;           // readable from several km out
		if ((i & 1) == 0) {        // even pads: white threshold
			p.r = 1.00f; p.g = 0.95f; p.b = 0.85f;
		} else {                   // odd pads: red approach
			p.r = 1.00f; p.g = 0.28f; p.b = 0.14f;
		}
		m_lights.push_back(p);
	}

	fprintf(stderr, "[OGLvBase] '%s': %zu pad lights\n",
	        name, m_lights.size());
}

void OGLvBase::Render(const float *vp, const VECTOR3 &camPos, const VECTOR3 & /*sunPos*/)
{
	if (!m_built) BuildLights();
	if (m_lights.empty() || !m_shaderMgr) return;

	// Rendering reuses the beacon shader (additive billboard textured quad).
	// That shader expects a vessel-relative transform; we pass an identity
	// model matrix because m_lights hold world-absolute positions. The
	// OGLBeaconArray path uses vesselRot/vesselPos to compose the billboard;
	// for bases we inline a minimal equivalent here so we don't have to
	// round-trip through a fake "vessel".

	// Visibility cut: bases further than ~500 km from the camera are too
	// small for the beacon to register as more than a single pixel; skip
	// the draw entirely so we don't pay for invisible geometry.
	const double cutoff = 5.0e5;

	// Delegate rendering to the shared OGLBeaconArray pipeline — the shader
	// and VAO are already loaded by OGLBeaconArray::InitShared() from the
	// client init path. PadLights hold global coordinates so we pass the
	// identity rotation + zero translation reference frame to the array.
	OGLBeaconArray local(m_shaderMgr);
	for (const PadLight &p : m_lights) {
		double dx = p.pos.x - camPos.x;
		double dy = p.pos.y - camPos.y;
		double dz = p.pos.z - camPos.z;
		if (dx * dx + dy * dy + dz * dz > cutoff * cutoff) continue;

		BeaconDef b;
		b.pos        = p.pos;   // global
		b.col        = { p.r, p.g, p.b };
		b.size       = p.size;
		b.period     = 0.0f;
		b.duration   = 1.0f;
		b.brightness = 1.0f;
		local.AddBeacon(b);
	}

	const MATRIX3 I    = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
	const VECTOR3 Zero = { 0, 0, 0 };
	local.Render(vp, camPos, I, Zero, 0.0);
}

} // namespace ogl

#endif // !_WIN32
