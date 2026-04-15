// gcCoreAPI.h - Stub header for non-D3D9Client builds
// The real gcCoreAPI.h is provided by the D3D9Client graphics extension.

#ifndef __GCCOREAPI_H
#define __GCCOREAPI_H

#include "OrbiterAPI.h"

// Stub gcCore class with empty methods
class gcCore {
public:
	static gcCore* GetInstance() { return nullptr; }
	// Empty class - no methods available without D3D9Client
};

#endif // __GCCOREAPI_H
