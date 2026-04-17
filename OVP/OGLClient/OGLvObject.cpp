// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef _WIN32

#include "OGLvObject.h"

namespace ogl {

OGLvObject::OGLvObject(OBJHANDLE hObj, ShaderMgr *shaderMgr)
	: m_hObj(hObj), m_shaderMgr(shaderMgr), m_gpos{0, 0, 0}
{
}

OGLvObject::~OGLvObject() {}

void OGLvObject::Update()
{
	if (m_hObj)
		oapiGetGlobalPos(m_hObj, &m_gpos);
}

} // namespace ogl

#endif // !_WIN32
