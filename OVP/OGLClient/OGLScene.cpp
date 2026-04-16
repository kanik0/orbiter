// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef _WIN32

#include "OGLScene.h"
#include "OGLShaderMgr.h"
#include "OGLCelSphere.h"
#include "OGLvPlanet.h"
#include "OGLvVessel.h"
#include <cstdio>
#include <cmath>

namespace ogl {

OGLScene::OGLScene(ShaderMgr *shaderMgr)
	: m_shaderMgr(shaderMgr), m_celSphere(nullptr),
	  m_initialized(false), m_objectsPopulated(false)
{
}

OGLScene::~OGLScene()
{
	Release();
}

void OGLScene::Init(const std::string &texturePath)
{
	m_texturePath = texturePath;

	// Initialize shared resources for visual object types
	OGLvPlanet::InitShared(m_shaderMgr, texturePath);
	OGLvVessel::InitShared(m_shaderMgr, texturePath);

	// Create celestial sphere
	m_celSphere = new OGLCelSphere(m_shaderMgr);
	m_celSphere->Init(4000);

	m_initialized = true;
}

void OGLScene::Release()
{
	for (auto *p : m_planets) delete p;
	m_planets.clear();
	for (auto *v : m_vessels) delete v;
	m_vessels.clear();

	delete m_celSphere;
	m_celSphere = nullptr;

	OGLvPlanet::ReleaseShared();
	OGLvVessel::ReleaseShared();

	m_initialized = false;
	m_objectsPopulated = false;
}

void OGLScene::PopulateObjects()
{
	if (m_objectsPopulated) return;
	m_objectsPopulated = true;

	// Enumerate celestial bodies (planets, moons, stars)
	DWORD nObj = oapiGetObjectCount();
	for (DWORD i = 0; i < nObj; i++) {
		OBJHANDLE hObj = oapiGetObjectByIndex(i);
		int type = oapiGetObjectType(hObj);
		if (type == OBJTP_PLANET || type == OBJTP_STAR) {
			OGLvPlanet *vp = new OGLvPlanet(hObj, m_shaderMgr);
			vp->LoadTexture(m_texturePath);
			vp->InitTiles(m_texturePath);
			m_planets.push_back(vp);
		}
	}
	fprintf(stderr, "[OGLScene] Populated %zu planets/stars\n", m_planets.size());
}

void OGLScene::BuildViewProjection(DWORD viewW, DWORD viewH,
                                    float *vp, VECTOR3 &camPos, MATRIX3 &camRot)
{
	oapiCameraGlobalPos(&camPos);
	oapiCameraRotationMatrix(&camRot);

	double fov = oapiCameraAperture() * 2.0;
	if (fov <= 0 || std::isnan(fov)) fov = 50.0 * 3.14159 / 180.0;
	double aspect = (double)viewW / (double)viewH;
	double nearPlane = 1.0, farPlane = 1e10;

	float f = 1.0f / tanf((float)fov * 0.5f);
	float A = (float)((farPlane + nearPlane) / (nearPlane - farPlane));
	float B = (float)(2.0 * farPlane * nearPlane / (nearPlane - farPlane));
	float proj[16] = {
		f / (float)aspect, 0, 0,  0,
		0,                 f, 0,  0,
		0,                 0, A, -1,
		0,                 0, B,  0
	};

	// View matrix: V = flipZ * GRot^T
	float view[16] = {
		(float)camRot.m11,  (float)camRot.m12, -(float)camRot.m13, 0,
		(float)camRot.m21,  (float)camRot.m22, -(float)camRot.m23, 0,
		(float)camRot.m31,  (float)camRot.m32, -(float)camRot.m33, 0,
		0, 0, 0, 1
	};

	// VP = P * V
	for (int col = 0; col < 4; col++)
		for (int row = 0; row < 4; row++) {
			float sum = 0;
			for (int k = 0; k < 4; k++)
				sum += proj[k * 4 + row] * view[col * 4 + k];
			vp[col * 4 + row] = sum;
		}
}

void OGLScene::RenderScene(DWORD viewW, DWORD viewH)
{
	if (!m_initialized) return;

	static int frameDbg = 0;
	if (frameDbg < 3) {
		fprintf(stderr, "[OGLScene] RenderScene frame %d (%ux%u)\n", frameDbg, viewW, viewH);
	}

	glViewport(0, 0, viewW, viewH);
	glClearColor(0.0f, 0.0f, 0.02f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	VECTOR3 camPos;
	MATRIX3 camRot;
	float vp[16];
	BuildViewProjection(viewW, viewH, vp, camPos, camRot);

	bool validCamera = !std::isnan(camPos.x) && !std::isnan(camPos.y) && !std::isnan(camPos.z);
	if (frameDbg < 10 || (!validCamera && frameDbg < 100)) {
		fprintf(stderr, "[OGLScene] Frame %d camera=(%.3g,%.3g,%.3g) valid=%d planets=%zu\n",
			frameDbg, camPos.x, camPos.y, camPos.z, validCamera, m_planets.size());
		frameDbg++;
	}
	// Skip only if truly NaN — zero camera is acceptable (render at origin)
	if (std::isnan(camPos.x)) return;

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_PROGRAM_POINT_SIZE);

	// Lazy-populate objects on first render frame
	PopulateObjects();

	// Get sun position
	VECTOR3 sunPos = {0, 0, 0};
	OBJHANDLE hSun = oapiGetObjectByName((char*)"Sun");
	if (hSun) oapiGetGlobalPos(hSun, &sunPos);

	// 1) Render starfield (at infinite distance)
	if (m_celSphere)
		m_celSphere->Render(vp);

	// 2) Render planets (depth test off for distance-normalized rendering)
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	for (auto *planet : m_planets)
		planet->Render(vp, camPos, sunPos);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);

	// 3) Render vessels
	// Rebuild vessel list each frame (vessels can be created/destroyed)
	for (auto *v : m_vessels) delete v;
	m_vessels.clear();
	DWORD nVessel = oapiGetVesselCount();
	for (DWORD i = 0; i < nVessel; i++) {
		OBJHANDLE hV = oapiGetVesselByIndex(i);
		if (hV) m_vessels.push_back(new OGLvVessel(hV, m_shaderMgr));
	}
	for (auto *vessel : m_vessels)
		vessel->Render(vp, camPos, sunPos);

	// 4) 2D overlay (handled by OGLClient after this call)
}

} // namespace ogl

#endif // !_WIN32
