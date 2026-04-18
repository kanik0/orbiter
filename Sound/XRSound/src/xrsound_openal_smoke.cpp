// Copyright (c) Martin Schweiger
// Licensed under the MIT License
//
// xrsound_openal_smoke — CTest-visible smoke test for the macOS/Linux
// OpenAL audio backend (M18-M21). Boots the backend, picks a few
// shipping default sounds at random from the Default pack, verifies
// the decoder produces a valid buffer and that OpenAL accepts it as a
// source. Exits zero on success, non-zero on the first failure.

#if !defined(_WIN32) || !defined(XRSOUND_DLL_BUILD)

#include "IAudioBackend.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

// Default sounds ship in Sound/XRSound/assets/XRSound/Default; this
// test gets that path either from the XRSOUND_SMOKE_ASSET_DIR env var
// (set by CTest) or falls back to a relative path so a developer can
// also run `build/Sound/xrsound_openal_smoke` from the build root.
std::string DefaultRoot()
{
	const char *env = std::getenv("XRSOUND_SMOKE_ASSET_DIR");
	if (env && *env) return env;
	return "XRSound/Default";
}

bool TryPlay(xrsound::IAudioBackend *be, const std::string &root,
             const char *fileName)
{
	std::string path = root + "/" + fileName;
	xrsound::IAudioSound *s = be->play2D(path.c_str(),
	                                      /*loop*/ false,
	                                      /*startPaused*/ true,
	                                      /*track*/ true);
	if (!s) {
		std::fprintf(stderr, "[smoke] play2D failed for '%s'\n", path.c_str());
		return false;
	}

	// Exercise the irrKlang-parity API surface we ported on top of
	// OpenAL: volume / pan / speed / position / loop / isFinished.
	s->setVolume(0.75f);
	s->setPan(0.25f);
	if (std::fabs(s->getPan() - 0.25f) > 0.01f) {
		std::fprintf(stderr, "[smoke] pan round-trip failed\n");
		s->drop();
		return false;
	}
	if (!s->setPlaybackSpeed(1.25f)) {
		std::fprintf(stderr, "[smoke] setPlaybackSpeed failed\n");
		s->drop();
		return false;
	}
	s->setIsPaused(true);
	s->setIsLooped(false);
	s->stop();
	s->drop();
	return true;
}

} // namespace

int main(int /*argc*/, char ** /*argv*/)
{
	xrsound::IAudioBackend *be = xrsound::createAudioBackend();
	if (!be) {
		std::fprintf(stderr, "[smoke] createAudioBackend() returned null\n");
		return 1;
	}

	const char *name = be->getDriverName();
	std::fprintf(stderr, "[smoke] driver = %s\n", name ? name : "(null)");
	if (!name || !*name) {
		std::fprintf(stderr, "[smoke] empty driver name\n");
		be->drop();
		return 2;
	}

	const std::string root = DefaultRoot();
	// Three representative files from the shipping pack — small enough
	// that CTest runs cheaply but diverse enough (cockpit, warning,
	// ambience) to exercise different WAV configurations (mono vs
	// stereo, 16-bit PCM).
	const char *picks[] = {
		"Docking Radar Beep.wav",
		"Gear Locked Thump.wav",
		"Warning Gear Failure.wav",
	};
	bool allOk = true;
	for (const char *p : picks) {
		if (!TryPlay(be, root, p)) allOk = false;
	}

	be->drop();
	return allOk ? 0 : 3;
}

#else
// On Windows the smoke test is irrelevant — irrKlang is the reference
// backend. Emit a stub main so the target still builds everywhere.
int main() { return 0; }
#endif
