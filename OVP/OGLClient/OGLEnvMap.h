// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// OGLEnvMap - IBL environment cubemaps for PBR vessel shading.
//
// Produces a GGX-prefiltered cubemap (256x256 x 6 faces, 5 mip levels) that
// vessel_pbr.frag samples via textureLod(uEnvMap, R, roughness * 4). The
// source is a lightweight analytical sky (space ambient + sun disc + halo)
// rendered into a capture cubemap (128x128) once at scene init; Refresh()
// can be called later with an updated sun direction when the scene changes
// noticeably. Building the prefilter chain at init keeps the per-frame cost
// at zero.

#ifndef __OGLENVMAP_H
#define __OGLENVMAP_H

#ifndef _WIN32
#include "OrbiterAPI.h"
#include <OpenGL/gl3.h>

namespace ogl {

class ShaderMgr;

class OGLEnvMap {
public:
	OGLEnvMap();
	~OGLEnvMap();

	// Allocate the capture / prefilter cubemaps, compile the shaders and
	// run the first bake. Returns false if any GL allocation fails; the
	// caller should then fall back to matHasEnvMap=0 on vessels.
	bool Init(ShaderMgr *shaderMgr, const VECTOR3 &sunDir);

	// Recompute the capture + prefilter cubemaps with a new sun direction.
	// Thread-safe only on the GL thread.
	void Refresh(const VECTOR3 &sunDir);

	GLuint GetPrefilterCube() const { return m_prefilterCube; }
	bool   IsReady() const { return m_prefilterCube != 0; }

	void Release();

private:
	ShaderMgr *m_shaderMgr;

	GLuint m_captureCube;
	GLuint m_prefilterCube;
	GLuint m_captureFBO;
	GLuint m_prefilterFBO;

	GLuint m_captureShader;
	GLuint m_prefilterShader;

	GLuint m_quadVAO;

	// Constants kept together for fast iteration later.
	static constexpr int kCaptureSize   = 128;
	static constexpr int kPrefilterSize = 256;
	static constexpr int kMipLevels     = 5;

	bool AllocateCube(GLuint &cube, int size, int mipLevels);
	void BakeCapture(const VECTOR3 &sunDir);
	void BakePrefilter();
};

} // namespace ogl

#endif // !_WIN32
#endif // __OGLENVMAP_H
