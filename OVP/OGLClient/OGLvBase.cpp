// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef _WIN32

#include "OGLvBase.h"
#include "OGLShaderMgr.h"
#include <cstdio>

namespace ogl {

OGLvBase::OGLvBase(OBJHANDLE hObj, ShaderMgr *shaderMgr)
	: OGLvObject(hObj, shaderMgr), m_meshesLoaded(false)
{
}

OGLvBase::~OGLvBase() {}

void OGLvBase::LoadMeshes()
{
	m_meshesLoaded = true;
	// Surface base mesh loading will be implemented when
	// the base mesh format and API are better understood.
	// For now, bases are rendered as part of the planet surface tiles.
}

void OGLvBase::Render(const float *vp, const VECTOR3 &camPos, const VECTOR3 &sunPos)
{
	if (!m_meshesLoaded) LoadMeshes();
	// Base rendering is currently handled by the tile system.
	// Explicit base mesh rendering will be added in a future iteration.
}

} // namespace ogl

#endif // !_WIN32
