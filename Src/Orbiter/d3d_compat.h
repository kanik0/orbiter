// D3D type compatibility layer for non-Windows platforms
// Provides minimal type definitions that the Orbiter core engine uses
// from Direct3D 7 headers (ddraw.h, d3d.h, d3dtypes.h)

#ifndef __D3D_COMPAT_H
#define __D3D_COMPAT_H

#ifdef _WIN32
#include <ddraw.h>
#include <d3d.h>
#else

#include "OrbiterPlatform.h"
#include <cmath>
#include <cstring>

typedef float D3DVALUE;
typedef float FLOAT;
typedef void  VOID;

#define D3DVAL(val) ((D3DVALUE)(val))

#define ZeroMemory(dest, size) memset(dest, 0, size)

typedef struct _D3DVECTOR {
	float x, y, z;
} D3DVECTOR;

typedef struct _D3DCOLORVALUE {
	float r, g, b, a;
} D3DCOLORVALUE;

typedef D3DCOLORVALUE* LPD3DCOLORVALUE;
typedef DWORD D3DCOLOR;

typedef struct _D3DVERTEX {
	float x, y, z;
	float nx, ny, nz;
	float tu, tv;
} D3DVERTEX;

typedef struct _D3DMATRIX {
	union {
		struct {
			float _11, _12, _13, _14;
			float _21, _22, _23, _24;
			float _31, _32, _33, _34;
			float _41, _42, _43, _44;
		};
		float m[4][4];
	};
} D3DMATRIX;

typedef struct _D3DMATERIAL7 {
	D3DCOLORVALUE diffuse;
	D3DCOLORVALUE ambient;
	D3DCOLORVALUE specular;
	D3DCOLORVALUE emissive;
	float power;
} D3DMATERIAL7;

typedef struct _D3DVIEWPORT7 {
	DWORD dwX, dwY;
	DWORD dwWidth, dwHeight;
	float dvMinZ, dvMaxZ;
} D3DVIEWPORT7;

// Flexible vertex format flags
#define D3DFVF_XYZ              0x002
#define D3DFVF_NORMAL           0x010
#define D3DFVF_TEX1             0x100
#define D3DFVF_TEX2             0x200
#define D3DFVF_TEXCOORDSIZE2(n) (0 << (2*n + 16))

// Opaque DirectDraw/Direct3D pointers (unused on non-Windows)
typedef void* LPDIRECT3DDEVICE7;
typedef void* LPDIRECTDRAWSURFACE7;
typedef void* LPDIRECTDRAW7;
typedef void* LPDIRECT3DVERTEXBUFFER7;

// DDSURFACEDESC2 stub
typedef struct _DDSURFACEDESC2 {
	DWORD dwSize;
	DWORD dwFlags;
	DWORD dwHeight;
	DWORD dwWidth;
	union {
		long lPitch;
		DWORD dwLinearSize;
	};
	DWORD dwBackBufferCount;
	DWORD dwMipMapCount;
	DWORD dwAlphaBitDepth;
	DWORD dwReserved;
	void* lpSurface;
	struct {
		DWORD dwSize;
		DWORD dwFlags;
		DWORD dwFourCC;
		DWORD dwRGBBitCount;
		DWORD dwRBitMask;
		DWORD dwGBitMask;
		DWORD dwBBitMask;
		DWORD dwRGBAlphaBitMask;
	} ddpfPixelFormat;
} DDSURFACEDESC2;

#endif // _WIN32
#endif // __D3D_COMPAT_H
