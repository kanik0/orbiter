// gcCoreAPI.h - Stub header for non-D3D9Client builds
// The real gcCoreAPI.h is provided by the D3D9Client graphics extension.
// This stub provides empty definitions so that LuaInterpreter compiles
// without the D3D9Client installed.

#ifndef __GCCOREAPI_H
#define __GCCOREAPI_H

#include "OrbiterAPI.h"

// Stub: no graphics core API available without D3D9Client
namespace gcCore {
	// Empty namespace - functions are not available
}

#endif // __GCCOREAPI_H
