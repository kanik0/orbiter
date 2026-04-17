// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// OGLTexture implementation - BMP and DDS texture loading

#ifndef _WIN32

#include "OGLTexture.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

// S3TC compressed texture format tokens (from GL_EXT_texture_compression_s3tc)
#ifndef GL_COMPRESSED_RGB_S3TC_DXT1_EXT
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT   0x83F0
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT  0x83F1
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT3_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT  0x83F2
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT  0x83F3
#endif

// DDS header flags
#define DDSD_MIPMAPCOUNT 0x20000
#define DDPF_FOURCC      0x04
#define DDPF_RGB         0x40
#define DDPF_ALPHAPIXELS 0x01

// ============================================================================
// Helper: read little-endian values from a byte buffer
// ============================================================================

static inline uint32_t ReadLE32(const unsigned char *p) {
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
	       ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static inline uint16_t ReadLE16(const unsigned char *p) {
	return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

// ============================================================================
// OGLTexture
// ============================================================================

OGLTexture::~OGLTexture() {
	Release();
}

void OGLTexture::Release() {
	if (texId) {
		glDeleteTextures(1, &texId);
		texId = 0;
	}
	width = height = 0;
}

// ============================================================================
// LoadTexture - auto-detect by extension
// ============================================================================

OGLTexture *OGLTexture::LoadTexture(const char *path) {
	if (!path || !path[0]) return nullptr;

	// Find file extension
	const char *ext = strrchr(path, '.');
	if (!ext) {
		fprintf(stderr, "[OGLTexture] No extension in '%s'\n", path);
		return nullptr;
	}

	if (strcasecmp(ext, ".dds") == 0) {
		return LoadDDS(path);
	} else if (strcasecmp(ext, ".bmp") == 0) {
		return LoadBMP(path);
	} else if (strcasecmp(ext, ".tex") == 0) {
		return LoadTEX(path);
	} else if (strcasecmp(ext, ".png") == 0) {
		return LoadPNG(path);
	} else {
		fprintf(stderr, "[OGLTexture] Unsupported format '%s'\n", ext);
		return nullptr;
	}
}

// ============================================================================
// LoadBMP - Windows BMP file loader
// Supports 8-bit (palette), 24-bit BGR, and 32-bit BGRA
// Handles bottom-up layout (positive height) by flipping rows
// Also supports RLE8 compression (type 1)
// ============================================================================

OGLTexture *OGLTexture::LoadBMP(const char *path) {
	FILE *fp = fopen(path, "rb");
	if (!fp) {
		fprintf(stderr, "[OGLTexture] Cannot open BMP '%s'\n", path);
		return nullptr;
	}

	// Read BITMAPFILEHEADER (14 bytes)
	unsigned char fileHdr[14];
	if (fread(fileHdr, 1, 14, fp) != 14) {
		fprintf(stderr, "[OGLTexture] BMP header too short '%s'\n", path);
		fclose(fp);
		return nullptr;
	}

	// Check magic "BM"
	if (fileHdr[0] != 'B' || fileHdr[1] != 'M') {
		fprintf(stderr, "[OGLTexture] Not a BMP file '%s'\n", path);
		fclose(fp);
		return nullptr;
	}

	uint32_t dataOffset = ReadLE32(fileHdr + 10);

	// Read BITMAPINFOHEADER (40 bytes)
	unsigned char infoHdr[40];
	if (fread(infoHdr, 1, 40, fp) != 40) {
		fprintf(stderr, "[OGLTexture] BMP info header too short '%s'\n", path);
		fclose(fp);
		return nullptr;
	}

	int32_t bmpWidth  = (int32_t)ReadLE32(infoHdr + 4);
	int32_t bmpHeight = (int32_t)ReadLE32(infoHdr + 8);
	uint16_t bpp      = ReadLE16(infoHdr + 14);
	uint32_t compression = ReadLE32(infoHdr + 16);

	bool flipVertical = (bmpHeight > 0); // bottom-up if positive
	uint32_t w = (uint32_t)abs(bmpWidth);
	uint32_t h = (uint32_t)abs(bmpHeight);

	if (w == 0 || h == 0 || w > 16384 || h > 16384) {
		fprintf(stderr, "[OGLTexture] BMP invalid dimensions %ux%u '%s'\n", w, h, path);
		fclose(fp);
		return nullptr;
	}

	// Allocate RGBA output
	std::vector<unsigned char> rgba(w * h * 4);

	if (bpp == 8) {
		// 8-bit palettized BMP
		// Read color table (256 entries, 4 bytes each: B, G, R, reserved)
		unsigned char palette[256 * 4];
		// Seek to after info header (might have already read 54 bytes total)
		fseek(fp, 14 + 40, SEEK_SET);
		if (fread(palette, 1, 256 * 4, fp) != 256 * 4) {
			fprintf(stderr, "[OGLTexture] BMP palette read failed '%s'\n", path);
			fclose(fp);
			return nullptr;
		}

		// Seek to pixel data
		fseek(fp, dataOffset, SEEK_SET);

		if (compression == 0) {
			// Uncompressed: each row is padded to 4-byte boundary
			uint32_t rowBytes = (w + 3) & ~3u; // w bytes per row, padded
			std::vector<unsigned char> rowBuf(rowBytes);

			for (uint32_t row = 0; row < h; row++) {
				uint32_t destRow = flipVertical ? (h - 1 - row) : row;
				if (fread(rowBuf.data(), 1, rowBytes, fp) != rowBytes) break;
				for (uint32_t col = 0; col < w; col++) {
					unsigned char idx = rowBuf[col];
					unsigned char *pe = palette + idx * 4;
					unsigned char *dst = rgba.data() + (destRow * w + col) * 4;
					dst[0] = pe[2]; // R (palette is B,G,R,X)
					dst[1] = pe[1]; // G
					dst[2] = pe[0]; // B
					dst[3] = 255;   // A
				}
			}
		} else if (compression == 1) {
			// RLE8 compressed
			uint32_t remainSize = 0;
			fseek(fp, 0, SEEK_END);
			long fileSize = ftell(fp);
			fseek(fp, dataOffset, SEEK_SET);
			remainSize = (uint32_t)(fileSize - dataOffset);

			std::vector<unsigned char> rleData(remainSize);
			size_t bytesRead = fread(rleData.data(), 1, remainSize, fp);

			uint32_t x = 0, y = 0;
			size_t pos = 0;
			while (pos + 1 < bytesRead && y < h) {
				unsigned char count = rleData[pos++];
				unsigned char value = rleData[pos++];

				if (count > 0) {
					// Encoded run: repeat 'value' count times
					for (int k = 0; k < count && x < w; k++) {
						uint32_t destRow = flipVertical ? (h - 1 - y) : y;
						unsigned char *pe = palette + value * 4;
						unsigned char *dst = rgba.data() + (destRow * w + x) * 4;
						dst[0] = pe[2]; dst[1] = pe[1]; dst[2] = pe[0]; dst[3] = 255;
						x++;
					}
				} else {
					// Escape: value determines action
					if (value == 0) {
						// End of line
						x = 0; y++;
					} else if (value == 1) {
						// End of bitmap
						break;
					} else if (value == 2) {
						// Delta
						if (pos + 1 < bytesRead) {
							x += rleData[pos++];
							y += rleData[pos++];
						}
					} else {
						// Absolute mode: 'value' literal bytes follow
						for (int k = 0; k < value && pos < bytesRead && x < w; k++) {
							uint32_t destRow = flipVertical ? (h - 1 - y) : y;
							unsigned char idx = rleData[pos++];
							unsigned char *pe = palette + idx * 4;
							unsigned char *dst = rgba.data() + (destRow * w + x) * 4;
							dst[0] = pe[2]; dst[1] = pe[1]; dst[2] = pe[0]; dst[3] = 255;
							x++;
						}
						// Absolute runs are word-aligned
						if (value & 1) pos++;
					}
				}
			}
		} else {
			fprintf(stderr, "[OGLTexture] BMP unsupported 8-bit compression %u '%s'\n", compression, path);
			fclose(fp);
			return nullptr;
		}

	} else if (bpp == 24) {
		// 24-bit BGR uncompressed
		fseek(fp, dataOffset, SEEK_SET);
		uint32_t rowBytes = (w * 3 + 3) & ~3u; // padded to 4
		std::vector<unsigned char> rowBuf(rowBytes);

		for (uint32_t row = 0; row < h; row++) {
			uint32_t destRow = flipVertical ? (h - 1 - row) : row;
			if (fread(rowBuf.data(), 1, rowBytes, fp) != rowBytes) break;
			for (uint32_t col = 0; col < w; col++) {
				unsigned char *src = rowBuf.data() + col * 3;
				unsigned char *dst = rgba.data() + (destRow * w + col) * 4;
				dst[0] = src[2]; // R (source is BGR)
				dst[1] = src[1]; // G
				dst[2] = src[0]; // B
				dst[3] = 255;    // A
			}
		}

	} else if (bpp == 32) {
		// 32-bit BGRA uncompressed
		fseek(fp, dataOffset, SEEK_SET);
		uint32_t rowBytes = w * 4;
		std::vector<unsigned char> rowBuf(rowBytes);

		for (uint32_t row = 0; row < h; row++) {
			uint32_t destRow = flipVertical ? (h - 1 - row) : row;
			if (fread(rowBuf.data(), 1, rowBytes, fp) != rowBytes) break;
			for (uint32_t col = 0; col < w; col++) {
				unsigned char *src = rowBuf.data() + col * 4;
				unsigned char *dst = rgba.data() + (destRow * w + col) * 4;
				dst[0] = src[2]; // R (source is BGRA)
				dst[1] = src[1]; // G
				dst[2] = src[0]; // B
				dst[3] = src[3]; // A
			}
		}

	} else {
		fprintf(stderr, "[OGLTexture] BMP unsupported bpp=%u '%s'\n", bpp, path);
		fclose(fp);
		return nullptr;
	}

	fclose(fp);

	// Create OpenGL texture
	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
	glGenerateMipmap(GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glBindTexture(GL_TEXTURE_2D, 0);

	OGLTexture *result = new OGLTexture();
	result->texId = tex;
	result->width = w;
	result->height = h;

	fprintf(stderr, "[OGLTexture] Loaded BMP %ux%u bpp=%u '%s'\n", w, h, bpp, path);
	return result;
}

// ============================================================================
// LoadDDS - DirectDraw Surface loader
// Supports DXT1, DXT3, DXT5 compressed, and uncompressed RGB/RGBA
// ============================================================================

OGLTexture *OGLTexture::LoadDDS(const char *path) {
	FILE *fp = fopen(path, "rb");
	if (!fp) {
		fprintf(stderr, "[OGLTexture] Cannot open DDS '%s'\n", path);
		return nullptr;
	}

	// Read magic
	unsigned char magic[4];
	if (fread(magic, 1, 4, fp) != 4 || memcmp(magic, "DDS ", 4) != 0) {
		fprintf(stderr, "[OGLTexture] Not a DDS file '%s'\n", path);
		fclose(fp);
		return nullptr;
	}

	// Read DDS header (124 bytes)
	unsigned char hdr[124];
	if (fread(hdr, 1, 124, fp) != 124) {
		fprintf(stderr, "[OGLTexture] DDS header too short '%s'\n", path);
		fclose(fp);
		return nullptr;
	}

	uint32_t hdrSize   = ReadLE32(hdr + 0);
	uint32_t flags     = ReadLE32(hdr + 4);
	uint32_t height    = ReadLE32(hdr + 8);
	uint32_t width     = ReadLE32(hdr + 12);
	uint32_t pitchOrLS = ReadLE32(hdr + 16);
	uint32_t mipCount  = ReadLE32(hdr + 24);

	(void)hdrSize;
	(void)pitchOrLS;

	if (!(flags & DDSD_MIPMAPCOUNT) || mipCount == 0)
		mipCount = 1;

	if (width == 0 || height == 0 || width > 16384 || height > 16384) {
		fprintf(stderr, "[OGLTexture] DDS invalid dimensions %ux%u '%s'\n", width, height, path);
		fclose(fp);
		return nullptr;
	}

	// Pixel format starts at offset 72 in the header
	uint32_t pfFlags  = ReadLE32(hdr + 76);
	char fourCC[5] = {};
	memcpy(fourCC, hdr + 80, 4);
	uint32_t rgbBits  = ReadLE32(hdr + 84);
	uint32_t rMask    = ReadLE32(hdr + 88);
	uint32_t gMask    = ReadLE32(hdr + 92);
	uint32_t bMask    = ReadLE32(hdr + 96);
	uint32_t aMask    = ReadLE32(hdr + 100);

	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);

	if (pfFlags & DDPF_FOURCC) {
		// Compressed format
		GLenum format = 0;
		uint32_t blockSize = 16; // bytes per 4x4 block

		if (memcmp(fourCC, "DXT1", 4) == 0) {
			format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
			blockSize = 8;
		} else if (memcmp(fourCC, "DXT3", 4) == 0) {
			format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
			blockSize = 16;
		} else if (memcmp(fourCC, "DXT5", 4) == 0) {
			format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
			blockSize = 16;
		} else {
			fprintf(stderr, "[OGLTexture] DDS unsupported FourCC '%.4s' '%s'\n", fourCC, path);
			glDeleteTextures(1, &tex);
			fclose(fp);
			return nullptr;
		}

		// Load each mip level
		uint32_t mw = width, mh = height;
		for (uint32_t level = 0; level < mipCount; level++) {
			uint32_t bw = (mw + 3) / 4;
			uint32_t bh = (mh + 3) / 4;
			uint32_t dataSize = bw * bh * blockSize;

			std::vector<unsigned char> data(dataSize);
			size_t bytesRead = fread(data.data(), 1, dataSize, fp);
			if (bytesRead < dataSize) {
				// Partial read is OK for the last mip level, pad with zeros
				memset(data.data() + bytesRead, 0, dataSize - bytesRead);
			}

			glCompressedTexImage2D(GL_TEXTURE_2D, level, format,
				mw, mh, 0, dataSize, data.data());

			mw = (mw > 1) ? mw / 2 : 1;
			mh = (mh > 1) ? mh / 2 : 1;
		}

		// If only one mip was provided, generate the rest
		if (mipCount <= 1) {
			// For compressed textures we cannot glGenerateMipmap, so just
			// set filtering to not use mipmaps
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		} else {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, mipCount - 1);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		}

	} else if (pfFlags & DDPF_RGB) {
		// Uncompressed RGB or RGBA
		bool hasAlpha = (pfFlags & DDPF_ALPHAPIXELS) != 0;
		uint32_t bytesPerPixel = rgbBits / 8;

		// 16-bit formats: convert to 32-bit RGBA on the fly
		if (rgbBits == 16) {
			uint32_t mw = width, mh = height;
			for (uint32_t level = 0; level < mipCount; level++) {
				uint32_t pxCount = mw * mh;
				std::vector<uint16_t> raw16(pxCount);
				fread(raw16.data(), 2, pxCount, fp);
				std::vector<unsigned char> rgba(pxCount * 4);
				// Detect format by mask: A1R5G5B5, R5G6B5, A4R4G4B4
				bool a1rgb5 = (rMask == 0x7C00 && gMask == 0x03E0 && bMask == 0x001F);
				bool a4rgb4 = (rMask == 0x0F00 && gMask == 0x00F0 && bMask == 0x000F);
				bool rgb565 = (rMask == 0xF800 && gMask == 0x07E0 && bMask == 0x001F);
				if (!a1rgb5 && !a4rgb4 && !rgb565) rgb565 = true; // default
				for (uint32_t i = 0; i < pxCount; i++) {
					uint16_t p = raw16[i];
					unsigned r, g, b, a;
					if (rgb565) {
						r = ((p >> 11) & 0x1F) * 255 / 31;
						g = ((p >> 5) & 0x3F) * 255 / 63;
						b = (p & 0x1F) * 255 / 31;
						a = 255;
					} else if (a1rgb5) {
						a = (p & 0x8000) ? 255 : 0;
						r = ((p >> 10) & 0x1F) * 255 / 31;
						g = ((p >> 5) & 0x1F) * 255 / 31;
						b = (p & 0x1F) * 255 / 31;
					} else { // a4rgb4
						a = ((p >> 12) & 0x0F) * 255 / 15;
						r = ((p >> 8) & 0x0F) * 255 / 15;
						g = ((p >> 4) & 0x0F) * 255 / 15;
						b = (p & 0x0F) * 255 / 15;
					}
					rgba[i*4+0] = r; rgba[i*4+1] = g; rgba[i*4+2] = b; rgba[i*4+3] = a;
				}
				glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA8, mw, mh, 0,
					GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
				mw = (mw > 1) ? mw / 2 : 1;
				mh = (mh > 1) ? mh / 2 : 1;
			}
			if (mipCount <= 1) {
				glGenerateMipmap(GL_TEXTURE_2D);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			} else {
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, mipCount - 1);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			}
			glBindTexture(GL_TEXTURE_2D, 0);
			fclose(fp);
			OGLTexture *result = new OGLTexture();
			result->texId = tex;
			result->width = width;
			result->height = height;
			fprintf(stderr, "[OGLTexture] Loaded DDS-RGB16 %ux%u '%s'\n", width, height, path);
			return result;
		}

		if (bytesPerPixel != 3 && bytesPerPixel != 4) {
			fprintf(stderr, "[OGLTexture] DDS unsupported RGB bits=%u '%s'\n", rgbBits, path);
			glDeleteTextures(1, &tex);
			fclose(fp);
			return nullptr;
		}

		uint32_t mw = width, mh = height;
		for (uint32_t level = 0; level < mipCount; level++) {
			uint32_t rowSize = mw * bytesPerPixel;
			uint32_t dataSize = rowSize * mh;
			std::vector<unsigned char> raw(dataSize);
			fread(raw.data(), 1, dataSize, fp);

			// Convert to RGBA
			std::vector<unsigned char> rgba(mw * mh * 4);
			for (uint32_t i = 0; i < mw * mh; i++) {
				unsigned char *src = raw.data() + i * bytesPerPixel;
				unsigned char *dst = rgba.data() + i * 4;

				// DDS uncompressed pixel data: channel order depends on masks
				// Common case: rMask=0xFF0000 gMask=0xFF00 bMask=0xFF (BGR order in memory on LE)
				// But we just read based on the masks
				uint32_t pixel = 0;
				for (uint32_t b = 0; b < bytesPerPixel; b++)
					pixel |= ((uint32_t)src[b]) << (b * 8);

				dst[0] = (unsigned char)((pixel & rMask) >> __builtin_ctz(rMask ? rMask : 1));
				dst[1] = (unsigned char)((pixel & gMask) >> __builtin_ctz(gMask ? gMask : 1));
				dst[2] = (unsigned char)((pixel & bMask) >> __builtin_ctz(bMask ? bMask : 1));
				dst[3] = hasAlpha ? (unsigned char)((pixel & aMask) >> __builtin_ctz(aMask ? aMask : 1)) : 255;
			}

			glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA, mw, mh, 0,
				GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());

			mw = (mw > 1) ? mw / 2 : 1;
			mh = (mh > 1) ? mh / 2 : 1;
		}

		if (mipCount <= 1) {
			glGenerateMipmap(GL_TEXTURE_2D);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		} else {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, mipCount - 1);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		}

	} else {
		fprintf(stderr, "[OGLTexture] DDS unsupported pixel format flags=0x%08x '%s'\n", pfFlags, path);
		glDeleteTextures(1, &tex);
		fclose(fp);
		return nullptr;
	}

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glBindTexture(GL_TEXTURE_2D, 0);

	fclose(fp);

	OGLTexture *result = new OGLTexture();
	result->texId = tex;
	result->width = width;
	result->height = height;

	fprintf(stderr, "[OGLTexture] Loaded DDS %ux%u fourcc='%.4s' mips=%u '%s'\n",
		width, height, fourCC, mipCount, path);
	return result;
}

// ============================================================================
// LoadDDSFromMemory - load a DDS texture from a memory buffer
// ============================================================================

OGLTexture *OGLTexture::LoadDDSFromMemory(const unsigned char *data, size_t size, const char *label) {
	if (!data || size < 128) return nullptr;
	if (memcmp(data, "DDS ", 4) != 0) return nullptr;

	const unsigned char *hdr = data + 4; // skip magic
	uint32_t flags     = ReadLE32(hdr + 4);
	uint32_t height    = ReadLE32(hdr + 8);
	uint32_t width     = ReadLE32(hdr + 12);
	uint32_t mipCount  = ReadLE32(hdr + 24);

	if (!(flags & DDSD_MIPMAPCOUNT) || mipCount == 0) mipCount = 1;
	if (width == 0 || height == 0 || width > 16384 || height > 16384) return nullptr;

	uint32_t pfFlags = ReadLE32(hdr + 76);
	char fourCC[5] = {};
	memcpy(fourCC, hdr + 80, 4);

	const unsigned char *pixelData = data + 4 + 124; // after magic + header
	size_t remaining = size - 128;

	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);

	if (pfFlags & DDPF_FOURCC) {
		GLenum format = 0;
		uint32_t blockSize = 16;
		if (memcmp(fourCC, "DXT1", 4) == 0) { format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT; blockSize = 8; }
		else if (memcmp(fourCC, "DXT3", 4) == 0) { format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT; blockSize = 16; }
		else if (memcmp(fourCC, "DXT5", 4) == 0) { format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT; blockSize = 16; }
		else { glDeleteTextures(1, &tex); return nullptr; }

		uint32_t mw = width, mh = height;
		size_t offset = 0;
		for (uint32_t level = 0; level < mipCount; level++) {
			uint32_t dataSize = ((mw + 3) / 4) * ((mh + 3) / 4) * blockSize;
			if (offset + dataSize > remaining) break;
			glCompressedTexImage2D(GL_TEXTURE_2D, level, format, mw, mh, 0, dataSize, pixelData + offset);
			offset += dataSize;
			mw = (mw > 1) ? mw / 2 : 1;
			mh = (mh > 1) ? mh / 2 : 1;
		}
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
			mipCount > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
		if (mipCount > 1) glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, mipCount - 1);
	} else {
		glDeleteTextures(1, &tex);
		return nullptr; // only compressed formats for now
	}

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glBindTexture(GL_TEXTURE_2D, 0);

	OGLTexture *result = new OGLTexture();
	result->texId = tex;
	result->width = width;
	result->height = height;
	if (label)
		fprintf(stderr, "[OGLTexture] Loaded DDS-mem %ux%u fourcc='%.4s' from '%s'\n",
			width, height, fourCC, label);
	return result;
}

// ============================================================================
// LoadTEX - Orbiter .tex container (concatenated DDS files)
// Loads the FIRST DDS texture found in the container.
// ============================================================================

OGLTexture *OGLTexture::LoadTEX(const char *path) {
	FILE *fp = fopen(path, "rb");
	if (!fp) return nullptr;

	fseek(fp, 0, SEEK_END);
	long fileSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	if (fileSize < 128) { fclose(fp); return nullptr; }

	std::vector<unsigned char> buf(fileSize);
	if ((long)fread(buf.data(), 1, fileSize, fp) != fileSize) {
		fclose(fp);
		return nullptr;
	}
	fclose(fp);

	// Scan for first 'DDS ' marker
	for (size_t i = 0; i + 128 <= (size_t)fileSize; i++) {
		if (memcmp(buf.data() + i, "DDS ", 4) == 0) {
			OGLTexture *tex = LoadDDSFromMemory(buf.data() + i, fileSize - i, path);
			if (tex) return tex;
		}
	}

	fprintf(stderr, "[OGLTexture] No DDS found in .tex '%s'\n", path);
	return nullptr;
}

// ============================================================================
// LoadPNG - using stb_image (single-header, included inline)
// ============================================================================

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO  // we do our own file I/O
#include "stb_image.h"

OGLTexture *OGLTexture::LoadPNG(const char *path) {
	FILE *fp = fopen(path, "rb");
	if (!fp) return nullptr;
	fseek(fp, 0, SEEK_END);
	long fileSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	std::vector<unsigned char> buf(fileSize);
	fread(buf.data(), 1, fileSize, fp);
	fclose(fp);

	int w, h, channels;
	unsigned char *pixels = stbi_load_from_memory(buf.data(), (int)fileSize, &w, &h, &channels, 4);
	if (!pixels) {
		fprintf(stderr, "[OGLTexture] PNG decode failed '%s'\n", path);
		return nullptr;
	}

	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	glGenerateMipmap(GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glBindTexture(GL_TEXTURE_2D, 0);

	stbi_image_free(pixels);

	OGLTexture *result = new OGLTexture();
	result->texId = tex;
	result->width = w;
	result->height = h;
	fprintf(stderr, "[OGLTexture] Loaded PNG %dx%d '%s'\n", w, h, path);
	return result;
}

#endif // !_WIN32
