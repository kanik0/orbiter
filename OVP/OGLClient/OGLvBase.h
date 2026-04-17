// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// OGLvBase - Surface base/spaceport rendering

#ifndef __OGLVBASE_H
#define __OGLVBASE_H

#ifndef _WIN32
#include "OGLvObject.h"
#include <string>
#include <vector>

namespace ogl {

class OGLvBase : public OGLvObject {
public:
	OGLvBase(OBJHANDLE hObj, ShaderMgr *shaderMgr);
	~OGLvBase() override;

	void Render(const float *vp, const VECTOR3 &camPos, const VECTOR3 &sunPos) override;

private:
	// Base meshes loaded from config
	struct BaseMesh {
		MESHHANDLE hMesh;
		VECTOR3 offset;
	};
	std::vector<BaseMesh> m_meshes;
	bool m_meshesLoaded;

	void LoadMeshes();
};

} // namespace ogl

#endif // !_WIN32
#endif // __OGLVBASE_H
