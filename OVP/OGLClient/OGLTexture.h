// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// OGLTexture - Texture loading for OpenGL graphics client (macOS/Linux)
// Supports BMP (8/24/32-bit) and DDS (DXT1/DXT3/DXT5 + uncompressed)

#ifndef __OGLTEXTURE_H
#define __OGLTEXTURE_H

#include <cstddef>
#ifndef _WIN32
#include <OpenGL/gl3.h>
#endif

struct OGLTexture {
	GLuint texId;
	unsigned int width;
	unsigned int height;

	OGLTexture() : texId(0), width(0), height(0) {}
	~OGLTexture();

	// Load from file, auto-detecting format by extension
	static OGLTexture *LoadTexture(const char *path);

	// Format-specific loaders
	static OGLTexture *LoadBMP(const char *path);
	static OGLTexture *LoadDDS(const char *path);
	static OGLTexture *LoadDDSFromMemory(const unsigned char *data, size_t size, const char *label = nullptr);
	static OGLTexture *LoadPNG(const char *path);

	// .tex container: loads first DDS texture from Orbiter's concatenated-DDS format
	static OGLTexture *LoadTEX(const char *path);

	// Release the GL texture resource
	void Release();
};

#endif // __OGLTEXTURE_H
