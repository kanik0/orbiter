// gcCoreAPI.h - Stub header for non-D3D9Client builds
// The real gcCoreAPI.h is provided by the D3D9Client graphics extension.

#ifndef __GCCOREAPI_H
#define __GCCOREAPI_H

#include "OrbiterAPI.h"

#ifndef _WIN32
#ifndef CAMERAHANDLE
typedef void* CAMERAHANDLE;
#endif
#endif

// Forward declarations for callback types used by custom camera
namespace oapi { class Sketchpad; }
typedef void (*__gcCustomCameraOverlay)(oapi::Sketchpad *pSkp, void *pParam);

// Stub gcCore class with empty methods
class gcCore {
public:
	static gcCore* GetInstance() { return nullptr; }

	// Custom camera stubs
	CAMERAHANDLE SetupCustomCamera(CAMERAHANDLE hCam, OBJHANDLE hVessel,
		VECTOR3 &pos, VECTOR3 &dir, VECTOR3 &up, double fov,
		SURFHANDLE hSurf, DWORD flags) { return nullptr; }
	int DeleteCustomCamera(CAMERAHANDLE hCam) { return -1; }
	int CustomCameraOnOff(CAMERAHANDLE hCam, bool bOn) { return -1; }
	int CustomCameraOverlay(CAMERAHANDLE hCam, __gcCustomCameraOverlay clbk, void *pParam) { return -1; }
};

// Stub for gcGetCoreInterface
inline gcCore* gcGetCoreInterface() { return nullptr; }

#endif // __GCCOREAPI_H
