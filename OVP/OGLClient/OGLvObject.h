// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// OGLvObject - Base class for all visual objects in the OGL scene

#ifndef __OGLVOBJECT_H
#define __OGLVOBJECT_H

#ifndef _WIN32
#include "OrbiterAPI.h"
#include <OpenGL/gl3.h>

namespace ogl {

class ShaderMgr;

class OGLvObject {
public:
	OGLvObject(OBJHANDLE hObj, ShaderMgr *shaderMgr);
	virtual ~OGLvObject();

	OBJHANDLE GetObjectHandle() const { return m_hObj; }

	// Update position/state from the simulation
	virtual void Update();

	// Render the object. vp = view-projection matrix, camPos = camera global pos
	virtual void Render(const float *vp, const VECTOR3 &camPos, const VECTOR3 &sunPos) = 0;

	// Position relative to camera (set by Update or Render)
	VECTOR3 GetGlobalPos() const { return m_gpos; }

protected:
	OBJHANDLE m_hObj;
	ShaderMgr *m_shaderMgr;
	VECTOR3 m_gpos; // global position
};

} // namespace ogl

#endif // !_WIN32
#endif // __OGLVOBJECT_H
