// Copyright (c) Martin Schweiger
// Licensed under the MIT License
//
// decoders_impl.cpp — single translation unit that instantiates the
// vendored single-header audio decoders used by OpenALBackend:
//
//   - dr_wav.h  (public domain / MIT-0, David Reid)
//   - dr_mp3.h  (public domain / MIT-0, David Reid)
//   - stb_vorbis.c (public domain / MIT, Sean Barrett)
//
// Keeping the implementation bodies here means other callers can
// #include the headers as declarations-only without dragging in ~20k
// lines of inlined code each time.

#if !defined(_WIN32) || !defined(XRSOUND_DLL_BUILD)

// --- dr_wav -----------------------------------------------------------------
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

// --- dr_mp3 -----------------------------------------------------------------
#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

// --- stb_vorbis -------------------------------------------------------------
// stb_vorbis ships as a .c file but we build it as C++ here; a couple of
// "unused function" warnings are the cost of keeping the single-TU
// strategy. The push-data / seek APIs are unused by XRSound (we load
// whole files), so we disable them to shave ~200 KB off the binary.
#define STB_VORBIS_NO_PUSHDATA_API
#define STB_VORBIS_NO_STDIO           // we'll use memory-based decoding only
// Re-enable STDIO since OpenALBackend uses stb_vorbis_decode_filename.
#undef STB_VORBIS_NO_STDIO
#include "stb_vorbis.c"

#endif // !_WIN32 || !XRSOUND_DLL_BUILD
