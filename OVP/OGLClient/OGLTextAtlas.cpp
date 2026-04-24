// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef _WIN32

#include "OGLTextAtlas.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

// Single-TU stb_truetype impl. STBTT_DEF resolves to `static` here so the
// implementation symbols never collide with the renamed copy ImGui carries
// internally (imstb_truetype.h with the same flag).
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

namespace ogl {

// Atlas geometry. 4096² × 1 byte = 16 MB of GPU memory. Empirical packing
// of the 4 fonts × 41 sizes × 95 codepoints used by Sketchpad consumes
// roughly 7-8 MB of glyph rasters; the doubling gives headroom for the
// largest sizes plus stbtt's row-shelf packing waste.
static constexpr int kAtlasW = 4096;
static constexpr int kAtlasH = 4096;
static constexpr int kPadding = 1;          // gutter between packed rects

OGLTextAtlas &OGLTextAtlas::Instance()
{
	static OGLTextAtlas s_instance;
	return s_instance;
}

OGLTextAtlas::~OGLTextAtlas()
{
	if (m_tex) {
		glDeleteTextures(1, &m_tex);
		m_tex = 0;
	}
}

bool OGLTextAtlas::RegisterFont(FontId id, const char *ttfPath)
{
	if ((int)id < 0 || (int)id >= (int)FontId::COUNT) return false;
	if (!ttfPath || !*ttfPath) return false;

	FILE *f = std::fopen(ttfPath, "rb");
	if (!f) return false;
	std::fseek(f, 0, SEEK_END);
	long n = std::ftell(f);
	std::fseek(f, 0, SEEK_SET);
	if (n <= 0) { std::fclose(f); return false; }

	FontData &fd = m_fonts[(int)id];
	fd.ttf.assign((size_t)n, 0);
	size_t got = std::fread(fd.ttf.data(), 1, (size_t)n, f);
	std::fclose(f);
	if (got != (size_t)n) { fd.ttf.clear(); fd.loaded = false; return false; }
	fd.loaded = true;
	return true;
}

bool OGLTextAtlas::HasFont(FontId id) const
{
	if ((int)id < 0 || (int)id >= (int)FontId::COUNT) return false;
	return m_fonts[(int)id].loaded;
}

int OGLTextAtlas::ClampSize(int size) const
{
	if (size < m_sizeMin) return m_sizeMin;
	if (size > m_sizeMax) return m_sizeMax;
	return size;
}

int OGLTextAtlas::Index(FontId id, int size, unsigned int cp) const
{
	const int sIdx = size - m_sizeMin;
	const int fIdx = (int)id;
	const int cpIdx = (int)(cp - kCpBase);
	return ((fIdx * m_sizeCount) + sIdx) * kCpCount + cpIdx;
}

int OGLTextAtlas::VMetricsIndex(FontId id, int size) const
{
	return (int)id * m_sizeCount + (size - m_sizeMin);
}

bool OGLTextAtlas::Build(int sizeMin, int sizeMax)
{
	if (sizeMin < 1)        sizeMin = 1;
	if (sizeMax < sizeMin)  sizeMax = sizeMin;

	m_sizeMin = sizeMin;
	m_sizeMax = sizeMax;
	m_sizeCount = sizeMax - sizeMin + 1;

	const int fontCount = (int)FontId::COUNT;
	const int totalGlyphs = fontCount * m_sizeCount * kCpCount;
	const int totalVMetric = fontCount * m_sizeCount;

	m_glyphs.assign(totalGlyphs, Glyph{});
	m_ascent.assign(totalVMetric, 0.0f);
	m_descent.assign(totalVMetric, 0.0f);
	m_lineHeight.assign(totalVMetric, 0.0f);

	std::vector<unsigned char> pixels((size_t)kAtlasW * kAtlasH, 0);

	stbtt_pack_context spc;
	if (!stbtt_PackBegin(&spc, pixels.data(), kAtlasW, kAtlasH,
	                     kAtlasW, kPadding, nullptr)) {
		return false;
	}
	// 1×1 oversample matches the Sketchpad use case (axis-aligned MFD
	// quads, no sub-pixel positioning); 2×2 would double atlas usage
	// for a benefit the existing pipeline can't show.
	stbtt_PackSetOversampling(&spc, 1, 1);

	bool ok = true;
	for (int fIdx = 0; fIdx < fontCount; fIdx++) {
		FontData &fd = m_fonts[fIdx];
		if (!fd.loaded) continue;

		stbtt_fontinfo info;
		if (!stbtt_InitFont(&info, fd.ttf.data(),
		                    stbtt_GetFontOffsetForIndex(fd.ttf.data(), 0))) {
			fd.loaded = false;
			continue;
		}

		std::vector<stbtt_packedchar> chardata((size_t)kCpCount);

		// hhea-based scaling — the conventional "size means ascent +
		// |descent|" mapping that ImGui and most UI text stacks use.
		// Glyphs whose bitmap extent slightly exceeds hhea (Lekton-Bold
		// ASCII span=1050 vs hhea sum=1000) can reach a few pixels past
		// the `size`-box; the only MFD code path that then shows
		// overlap is AscentMFD, whose rows are drawn exactly `ch`
		// pixels apart. That overlap was already present in the ImGui-
		// backed build — fixing it would require either shrinking every
		// glyph globally or sending MFD-specific metrics, both of which
		// hurt the common case more than the fringe one.
		int hhea_a, hhea_d, hhea_lg;
		stbtt_GetFontVMetrics(&info, &hhea_a, &hhea_d, &hhea_lg);

		for (int size = sizeMin; size <= sizeMax; size++) {
			std::memset(chardata.data(), 0,
			            chardata.size() * sizeof(stbtt_packedchar));

			if (!stbtt_PackFontRange(&spc, fd.ttf.data(), 0, (float)size,
			                         (int)kCpBase, kCpCount,
			                         chardata.data())) {
				ok = false;
				continue;
			}

			float scale      = stbtt_ScaleForPixelHeight(&info, (float)size);
			float ascent     = (float)hhea_a * scale;
			float descent    = (float)hhea_d * scale;
			float lineHeight = (float)(hhea_a - hhea_d + hhea_lg) * scale;

			const int vIdx = VMetricsIndex((FontId)fIdx, size);
			m_ascent[vIdx]     = ascent;
			m_descent[vIdx]    = descent;
			m_lineHeight[vIdx] = lineHeight;

			for (int c = 0; c < kCpCount; c++) {
				const stbtt_packedchar &pc = chardata[(size_t)c];
				Glyph &g = m_glyphs[(size_t)Index((FontId)fIdx, size,
				                                  kCpBase + (unsigned)c)];
				// stbtt anchors yoff/yoff2 to the baseline (negative for
				// glyphs that ascend above it). Sketchpad's existing pen
				// is at the top-of-line ("ascent" line), so we shift
				// every glyph down by `ascent` to keep the historical
				// `gy0 = py + glyph.y0` formula.
				g.x0 = pc.xoff;
				g.y0 = pc.yoff  + ascent;
				g.x1 = pc.xoff2;
				g.y1 = pc.yoff2 + ascent;
				g.u0 = (float)pc.x0 / (float)kAtlasW;
				g.v0 = (float)pc.y0 / (float)kAtlasH;
				g.u1 = (float)pc.x1 / (float)kAtlasW;
				g.v1 = (float)pc.y1 / (float)kAtlasH;
				g.advanceX = pc.xadvance;
			}
		}
	}
	stbtt_PackEnd(&spc);

	if (m_tex) glDeleteTextures(1, &m_tex);
	glGenTextures(1, &m_tex);
	glBindTexture(GL_TEXTURE_2D, m_tex);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, kAtlasW, kAtlasH, 0,
	             GL_RED, GL_UNSIGNED_BYTE, pixels.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	// sketchpad.frag MODE_TEXT samples `.a` for the glyph mask. Swizzle
	// the single red channel through the alpha tap so the existing
	// shader keeps working unchanged.
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_ONE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_ONE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_ONE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_RED);
	glBindTexture(GL_TEXTURE_2D, 0);

	m_built = ok;
	return ok;
}

const Glyph *OGLTextAtlas::GetGlyph(FontId id, int size,
                                    unsigned int codepoint) const
{
	if (!m_built) return nullptr;
	if ((int)id < 0 || (int)id >= (int)FontId::COUNT) return nullptr;
	if (!m_fonts[(int)id].loaded) return nullptr;
	if (codepoint < kCpBase || codepoint >= kCpEnd) return nullptr;
	const int s = ClampSize(size);
	const Glyph &g = m_glyphs[(size_t)Index(id, s, codepoint)];
	// Empty advance flags glyphs the packer skipped (e.g. control code).
	if (g.advanceX == 0.0f && g.x1 == g.x0) return nullptr;
	return &g;
}

float OGLTextAtlas::GetTextWidth(FontId id, int size,
                                 const char *s, const char *end) const
{
	if (!s || !m_built) return 0.0f;
	if (!end) end = s + std::strlen(s);

	const int sz = ClampSize(size);
	float w = 0.0f;
	for (const char *p = s; p < end; ++p) {
		unsigned int cp = (unsigned char)*p;
		if (cp < kCpBase || cp >= kCpEnd) continue;
		const Glyph *g = GetGlyph(id, sz, cp);
		if (g) w += g->advanceX;
	}
	return w;
}

void OGLTextAtlas::GetVMetrics(FontId id, int size, float *ascent,
                               float *descent, float *lineHeight) const
{
	if (ascent)     *ascent     = 0.0f;
	if (descent)    *descent    = 0.0f;
	if (lineHeight) *lineHeight = (float)size;
	if (!m_built) return;
	if ((int)id < 0 || (int)id >= (int)FontId::COUNT) return;
	if (!m_fonts[(int)id].loaded) return;

	const int s = ClampSize(size);
	const int v = VMetricsIndex(id, s);
	if (ascent)     *ascent     = m_ascent[v];
	if (descent)    *descent    = m_descent[v];
	if (lineHeight) *lineHeight = m_lineHeight[v];
}

} // namespace ogl

#endif // !_WIN32
