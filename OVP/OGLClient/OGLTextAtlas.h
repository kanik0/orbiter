// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// OGLTextAtlas — process-lifetime bitmap font atlas for OGLSketchpad text.
//
// The Sketchpad text path (MFD / HUD / VC label / 2D panel text) used to
// sample ImGui's dynamic font atlas. ImGui 1.92 lazily bakes glyphs the
// first time a (font, size, codepoint) triple is drawn; later bakes can
// repack the atlas, and ImFontGlyph::U0..V1 are documented as valid only
// for the current ImFontAtlas::TexRef. On macOS ARM64 (Metal-wrapped GL
// 4.1) that repack consistently leaves stale UVs on a subset of earlier
// glyphs — those quads sample empty texels and render as whitespace
// while their AdvanceX still moves the pen ("MODE SELECT" → "M E SE E T").
// See #128 for the full root-cause analysis.
//
// OGLTextAtlas removes the dynamic-bake path entirely for Sketchpad text:
// every printable ASCII codepoint is rasterised at every integer point
// size 8..48 into a single GL_R8 texture at startup, the texture id never
// changes, and (u, v, advance) for every glyph is computed once and read
// back unchanged for the rest of the process. ImGui keeps its own atlas
// for dialogs / Launchpad text — that path goes through
// ImGui_ImplOpenGL3_RenderDrawData which does correctly re-upload on
// repack.

#ifndef __OGLTEXTATLAS_H
#define __OGLTEXTATLAS_H

#ifndef _WIN32

#include <OpenGL/gl3.h>
#include <vector>

namespace ogl {

// Sketchpad-relevant font slots. Mirrors the four .ttf files registered by
// DlgMgr::InitImGui (Roboto / Cousine / Lekton-Bold / Architext). Icons
// are intentionally absent — Sketchpad never renders FontAwesome glyphs.
enum class FontId : int {
	DEFAULT    = 0,   // proportional UI font (Roboto-Medium by default)
	CONSOLE    = 1,   // proportional console font (Cousine-Regular)
	MONO       = 2,   // fixed-width font (Lekton-Bold)
	MANUSCRIPT = 3,   // decorative manuscript font (Architext)
	COUNT      = 4,
};

// All metrics in target-pixel space. (x0, y0)-(x1, y1) is the quad
// rectangle in pen-relative coordinates (top-left of the quad relative
// to the pen baseline-top); (u0, v0)-(u1, v1) the matching uv rect on
// the shared atlas texture.
struct Glyph {
	float u0, v0, u1, v1;
	float x0, y0, x1, y1;
	float advanceX;
};

class OGLTextAtlas {
public:
	static OGLTextAtlas &Instance();

	// Load a TTF file and stash its bytes for the upcoming Build() pass.
	// Returns false on missing file — caller should fall back to a slot
	// that did load. Safe to call again to swap a font; Build() must
	// then be re-run.
	bool RegisterFont(FontId id, const char *ttfPath);

	// Whether RegisterFont has been called for this slot AND the file
	// existed. Lets call sites pick a fallback (e.g. CONSOLE → DEFAULT).
	bool HasFont(FontId id) const;

	// Rasterise every printable ASCII codepoint in [0x20, 0x7F) at every
	// integer point size in [sizeMin, sizeMax] for every registered font
	// into a single GL_R8 texture. Idempotent: a second call frees the
	// previous texture and re-bakes from the currently registered fonts.
	// Returns false if the resulting glyph set didn't fit the atlas
	// (caller should LOGOUT and fall back to lazy bakes — but in practice
	// the 4096² atlas accommodates 4 fonts × 41 sizes × 95 codepoints
	// with headroom).
	bool Build(int sizeMin = 8, int sizeMax = 48);

	// Return the cached glyph for (font, size, codepoint). `size` is
	// rounded to the nearest baked integer. Returns nullptr when the
	// codepoint is outside the baked ASCII range, when the font isn't
	// registered, or when Build() hasn't been called.
	const Glyph *GetGlyph(FontId id, int size, unsigned int codepoint) const;

	// Pixel width of a UTF-8 string at (font, size). `end` defaults to
	// the NUL terminator. Honours the same size-rounding as GetGlyph.
	float GetTextWidth(FontId id, int size, const char *s,
	                   const char *end = nullptr) const;

	// Vertical metrics for (font, size). All three out params are pixels
	// from the pen baseline-top. Any may be null.
	void GetVMetrics(FontId id, int size, float *ascent,
	                 float *descent, float *lineHeight) const;

	// Shared atlas texture, valid until Build() is called again or the
	// process exits. 0 before the first successful Build().
	GLuint GetTextureID() const { return m_tex; }

private:
	OGLTextAtlas() = default;
	~OGLTextAtlas();
	OGLTextAtlas(const OGLTextAtlas &) = delete;
	OGLTextAtlas &operator=(const OGLTextAtlas &) = delete;

	// Codepoint range baked into the atlas. Latin-1 supplement is left
	// out for now; the only Sketchpad strings that need it are localised
	// vessel labels, which the macOS port doesn't ship yet.
	static constexpr unsigned int kCpBase = 0x20;     // inclusive
	static constexpr unsigned int kCpEnd  = 0x7F;     // exclusive
	static constexpr int kCpCount = (int)(kCpEnd - kCpBase);

	int ClampSize(int size) const;
	int Index(FontId id, int size, unsigned int cp) const;
	int VMetricsIndex(FontId id, int size) const;

	struct FontData {
		std::vector<unsigned char> ttf;   // owned TTF bytes
		bool loaded = false;
	};

	FontData m_fonts[(int)FontId::COUNT];

	int m_sizeMin = 8;
	int m_sizeMax = 48;
	int m_sizeCount = 0;       // sizeMax - sizeMin + 1, cached after Build

	std::vector<Glyph> m_glyphs;       // dense [(font, size, cp)] table
	std::vector<float> m_ascent;       // [(font, size)]
	std::vector<float> m_descent;
	std::vector<float> m_lineHeight;

	GLuint m_tex = 0;
	bool   m_built = false;
};

} // namespace ogl

#endif // !_WIN32
#endif // __OGLTEXTATLAS_H
