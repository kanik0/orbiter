// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// OGLvBase - Surface base / spaceport visualisation.
//
// Each Orbiter planet exposes a list of bases (spaceports) through the
// oapiGetBase* API. OGLvBase queries the pad positions for one base and
// keeps a list of world-space landing beacons so the render pass can draw
// them as additive billboards during low-altitude approaches — the
// feature the Windows reference drives through its RunwayLights module.
//
// Mesh-level base geometry (tarmacs, hangars) is still handled by the
// planet surface tile texture; a dedicated mesh loader would require the
// .bse base-file parser that only lands with a full asset pack port and
// is explicitly out of scope here.

#ifndef __OGLVBASE_H
#define __OGLVBASE_H

#ifndef _WIN32
#include "OGLvObject.h"
#include <OpenGL/gl3.h>
#include <vector>

namespace ogl {

class OGLvBase : public OGLvObject {
public:
	OGLvBase(OBJHANDLE hBase, ShaderMgr *shaderMgr);
	~OGLvBase() override;

	void Render(const float *vp, const VECTOR3 &camPos, const VECTOR3 &sunPos) override;

private:
	// Pad-derived landing beacons, in planet-local world metres (already
	// rotated into the planet's global frame at BuildLights() time).
	struct PadLight {
		VECTOR3 pos;
		float   r, g, b;
		float   size;
	};
	std::vector<PadLight> m_lights;
	bool m_built;

	void BuildLights();
};

} // namespace ogl

#endif // !_WIN32
#endif // __OGLVBASE_H
