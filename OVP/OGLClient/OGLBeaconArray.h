// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// OGLBeaconArray - Navigation light and runway light rendering

#ifndef __OGLBEACONARRAY_H
#define __OGLBEACONARRAY_H

#ifndef _WIN32
#include "OrbiterAPI.h"
#include <OpenGL/gl3.h>
#include <vector>

namespace ogl {

class ShaderMgr;

struct BeaconDef {
	VECTOR3 pos;       // position relative to vessel
	VECTOR3 col;       // RGB color
	float size;        // beacon size [m]
	float period;      // blink period [s] (0 = always on)
	float duration;    // on-duration fraction [0-1]
	float brightness;  // intensity [0-1]
};

class OGLBeaconArray {
public:
	OGLBeaconArray(ShaderMgr *shaderMgr);
	~OGLBeaconArray();

	static void InitShared(ShaderMgr *shaderMgr);
	static void ReleaseShared();

	void AddBeacon(const BeaconDef &b);
	void Render(const float *vp, const VECTOR3 &camPos, const MATRIX3 &vesselRot,
	            const VECTOR3 &vesselPos, double simT);

private:
	ShaderMgr *m_shaderMgr;
	std::vector<BeaconDef> m_beacons;

	static GLuint s_shader;
	static GLuint s_vao, s_vbo;
	static bool s_initialized;
};

} // namespace ogl

#endif // !_WIN32
#endif // __OGLBEACONARRAY_H
