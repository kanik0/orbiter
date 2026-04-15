// D3D type stubs for building meshc on non-Windows platforms
// Delegates to the core d3d_compat.h and adds meshc-specific extras

#ifndef __D3D_STUB_H
#define __D3D_STUB_H

#include "d3d_compat.h"

// Extra constants/defines needed by meshc Mesh.cpp render functions
#define D3DSTATUS_DEFAULT     0
#define D3DRENDERSTATE_ZBIAS  0
#define D3DRENDERSTATE_WRAP0  0
#define D3DWRAP_U             0x01
#define D3DWRAP_V             0x02
#define D3DFVF_VERTEX         (D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1)
#define D3DPT_TRIANGLELIST    0

#endif
