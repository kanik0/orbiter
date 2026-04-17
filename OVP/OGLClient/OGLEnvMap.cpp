// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef _WIN32

#include "OGLEnvMap.h"
#include "OGLShaderMgr.h"
#include "OGLSurface.h"  // ogl::FBOBinder

#include <cstdio>

namespace ogl {

OGLEnvMap::OGLEnvMap()
	: m_shaderMgr(nullptr),
	  m_captureCube(0), m_prefilterCube(0),
	  m_captureFBO(0), m_prefilterFBO(0),
	  m_captureShader(0), m_prefilterShader(0),
	  m_quadVAO(0)
{}

OGLEnvMap::~OGLEnvMap() { Release(); }

bool OGLEnvMap::AllocateCube(GLuint &cube, int size, int mipLevels)
{
	glGenTextures(1, &cube);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cube);

	// Allocate storage for each mip level of every face. glTexImage2D with
	// null data is sufficient on GL 4.1 core — we don't upload source data,
	// the capture/prefilter passes paint directly into each face.
	for (int mip = 0; mip < mipLevels; mip++) {
		int w = size >> mip;
		if (w < 1) w = 1;
		for (int face = 0; face < 6; face++) {
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
			             mip, GL_RGBA16F, w, w, 0, GL_RGBA, GL_HALF_FLOAT, nullptr);
		}
	}

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER,
	                mipLevels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R,     GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL,  mipLevels - 1);
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
	return cube != 0;
}

bool OGLEnvMap::Init(ShaderMgr *shaderMgr, const VECTOR3 &sunDir)
{
	m_shaderMgr = shaderMgr;

	if (!AllocateCube(m_captureCube,   kCaptureSize,   1))           return false;
	if (!AllocateCube(m_prefilterCube, kPrefilterSize, kMipLevels))  return false;

	glGenFramebuffers(1, &m_captureFBO);
	glGenFramebuffers(1, &m_prefilterFBO);
	glGenVertexArrays(1, &m_quadVAO);

	m_captureShader   = m_shaderMgr->LoadProgram("env_capture",
	                                             "env_capture.vert", "env_capture.frag");
	m_prefilterShader = m_shaderMgr->LoadProgram("env_prefilter",
	                                             "env_capture.vert", "env_prefilter.frag");
	if (!m_captureShader || !m_prefilterShader) {
		fprintf(stderr, "[OGLEnvMap] shader load failed\n");
		Release();
		return false;
	}

	Refresh(sunDir);
	fprintf(stderr,
	        "[OGLEnvMap] baked capture=%dpx prefilter=%dpx mips=%d\n",
	        kCaptureSize, kPrefilterSize, kMipLevels);
	return true;
}

void OGLEnvMap::Refresh(const VECTOR3 &sunDir)
{
	BakeCapture(sunDir);
	BakePrefilter();
}

void OGLEnvMap::BakeCapture(const VECTOR3 &sunDir)
{
	double len = std::sqrt(sunDir.x * sunDir.x + sunDir.y * sunDir.y + sunDir.z * sunDir.z);
	float sd[3] = {
		float(len > 1e-6 ? sunDir.x / len : 0.0),
		float(len > 1e-6 ? sunDir.y / len : 1.0),
		float(len > 1e-6 ? sunDir.z / len : 0.0)
	};

	// Dark-space ambient — tuned so that fully shadowed dielectric surfaces
	// don't go pure black when reflecting the environment.
	const float spaceAmbient[3] = { 0.005f, 0.006f, 0.010f };
	const float sunColor[3]     = { 1.0f, 0.97f, 0.92f };

	glBindFramebuffer(GL_FRAMEBUFFER, m_captureFBO);
	glUseProgram(m_captureShader);
	glUniform3fv(m_shaderMgr->GetUniformLoc(m_captureShader, "uSunDir"),       1, sd);
	glUniform3fv(m_shaderMgr->GetUniformLoc(m_captureShader, "uSunColor"),     1, sunColor);
	glUniform3fv(m_shaderMgr->GetUniformLoc(m_captureShader, "uSpaceAmbient"), 1, spaceAmbient);

	glViewport(0, 0, kCaptureSize, kCaptureSize);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glBindVertexArray(m_quadVAO);

	for (int face = 0; face < 6; face++) {
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		                       GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
		                       m_captureCube, 0);
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			fprintf(stderr, "[OGLEnvMap] capture FBO incomplete, face %d\n", face);
			continue;
		}
		glUniform1i(m_shaderMgr->GetUniformLoc(m_captureShader, "uFace"), face);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	}

	glBindVertexArray(0);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glUseProgram(0);
}

void OGLEnvMap::BakePrefilter()
{
	glBindFramebuffer(GL_FRAMEBUFFER, m_prefilterFBO);
	glUseProgram(m_prefilterShader);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, m_captureCube);
	glUniform1i(m_shaderMgr->GetUniformLoc(m_prefilterShader, "uCapture"), 0);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glBindVertexArray(m_quadVAO);

	for (int mip = 0; mip < kMipLevels; mip++) {
		int   size      = kPrefilterSize >> mip;
		if (size < 1) size = 1;
		// 5-mip chain → roughness 0, 0.25, 0.5, 0.75, 1.0.
		float roughness = (kMipLevels <= 1) ? 0.0f : float(mip) / float(kMipLevels - 1);
		int   samples   = 16 + mip * 12;   // more samples on blurrier mips

		glViewport(0, 0, size, size);
		glUniform1f(m_shaderMgr->GetUniformLoc(m_prefilterShader, "uRoughness"), roughness);
		glUniform1i(m_shaderMgr->GetUniformLoc(m_prefilterShader, "uSamples"),   samples);

		for (int face = 0; face < 6; face++) {
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			                       GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
			                       m_prefilterCube, mip);
			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
				fprintf(stderr, "[OGLEnvMap] prefilter FBO incomplete (mip %d face %d)\n",
				        mip, face);
				continue;
			}
			glUniform1i(m_shaderMgr->GetUniformLoc(m_prefilterShader, "uFace"), face);
			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		}
	}

	glBindVertexArray(0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glUseProgram(0);
}

void OGLEnvMap::Release()
{
	if (m_captureCube)   { glDeleteTextures(1, &m_captureCube);     m_captureCube = 0; }
	if (m_prefilterCube) { glDeleteTextures(1, &m_prefilterCube);   m_prefilterCube = 0; }
	if (m_captureFBO)    { glDeleteFramebuffers(1, &m_captureFBO);  m_captureFBO = 0; }
	if (m_prefilterFBO)  { glDeleteFramebuffers(1, &m_prefilterFBO); m_prefilterFBO = 0; }
	if (m_quadVAO)       { glDeleteVertexArrays(1, &m_quadVAO);     m_quadVAO = 0; }
	m_captureShader = m_prefilterShader = 0; // owned by ShaderMgr
}

} // namespace ogl

#endif // !_WIN32
