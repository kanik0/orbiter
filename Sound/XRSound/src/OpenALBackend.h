// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// OpenAL audio backend for XRSound on macOS/Linux.
//
// Implements xrsound::IAudioBackend / xrsound::IAudioSound against
// OpenAL Soft (Homebrew `openal-soft` on macOS, `libopenal-dev` on
// Linux). Apple's bundled <OpenAL/al.h> framework is deprecated; we
// intentionally link against the portable `openal-soft` distribution
// both for feature parity (EFX, multi-channel) and to silence the
// deprecation warnings in the macOS 14+ SDK.

#ifndef __OPENALBACKEND_H
#define __OPENALBACKEND_H

#if !defined(_WIN32) || !defined(XRSOUND_DLL_BUILD)

#include "IAudioBackend.h"
#include <AL/al.h>
#include <AL/alc.h>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace xrsound {

// Concrete playback handle. OpenAL's source/buffer split maps 1:1 onto
// irrKlang's ISound: one buffer (shared, keyed by filename), one source
// per play2D invocation. Destruction frees the source but leaves the
// cached buffer alive for subsequent plays of the same file.
class OpenALSound : public IAudioSound {
public:
	OpenALSound(ALuint source, ALuint buffer, float durationSeconds);
	~OpenALSound() override;

	void drop() override;
	void stop() override;
	bool isFinished() override;

	void setVolume(float volume) override;
	void setIsLooped(bool loop) override;
	void setIsPaused(bool pause) override;

	void  setPan(float pan) override;
	float getPan() override;

	bool  setPlaybackSpeed(float speed) override;
	float getPlaybackSpeed() override;

	bool         setPlayPosition(unsigned int positionMs) override;
	unsigned int getPlayPosition() override;

private:
	ALuint m_source;
	ALuint m_buffer;      // kept for size reporting; not owned
	float  m_durationSec;
	float  m_pan;
	float  m_volume;
};

class OpenALEngine : public IAudioBackend {
public:
	OpenALEngine();
	~OpenALEngine() override;

	// Open device + context; must be called before any play2D.
	bool init();

	// IAudioBackend overrides ---------------------------------------------
	void         drop() override;
	IAudioSound *play2D(const char *filename, bool loop = false,
	                    bool startPaused = false,
	                    bool track = false) override;
	void         update() override;
	const char  *getDriverName() override;

private:
	// Shared WAV buffer cache (filename → AL buffer id). Each play2D
	// creates a fresh AL source that references an entry here.
	ALuint LoadFile(const char *filename, float *outDurationSec);

	// Parse a RIFF/WAVE file into raw PCM. Returns false on any header
	// mismatch or IO failure; leaves outputs untouched.
	bool ParseWav(const char *filename, std::vector<uint8_t> &data,
	              int &channels, int &sampleRate, int &bitsPerSample);

	ALCdevice  *m_device;
	ALCcontext *m_context;
	bool        m_initialized;

	std::string m_driverName;  // cached from alcGetString(ALC_DEVICE_SPECIFIER)
	std::map<std::string, ALuint> m_bufferCache;
};

} // namespace xrsound

#endif // !_WIN32 || !XRSOUND_DLL_BUILD
#endif // __OPENALBACKEND_H
