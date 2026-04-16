// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// OpenAL audio backend for XRSound on macOS/Linux
// Replaces irrKlang with Apple's built-in OpenAL framework

#ifndef __OPENALBACKEND_H
#define __OPENALBACKEND_H

#ifndef _WIN32

#ifdef __APPLE__
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#endif

#include <string>
#include <map>
#include <vector>
#include <cstdint>

namespace xrsound {

// A loaded audio buffer
struct AudioBuffer {
	ALuint bufferId;
	int channels;
	int sampleRate;
	int bitsPerSample;
	float duration; // seconds
};

// An active sound source
struct AudioSource {
	ALuint sourceId;
	ALuint bufferId;
	bool isLooping;
	float volume;
	bool isPaused;
};

class OpenALEngine {
public:
	OpenALEngine();
	~OpenALEngine();

	// Initialize the OpenAL device and context
	bool Init();

	// Shutdown and release all resources
	void Shutdown();

	// Load a WAV file into an OpenAL buffer. Returns buffer ID or 0 on failure.
	ALuint LoadWav(const char *filename);

	// Play a loaded buffer. Returns source ID or 0 on failure.
	ALuint Play(ALuint bufferId, bool loop, float volume);

	// Stop a playing source
	void Stop(ALuint sourceId);

	// Set volume [0-1]
	void SetVolume(ALuint sourceId, float volume);

	// Pause/resume
	void Pause(ALuint sourceId);
	void Resume(ALuint sourceId);

	// Check if source has finished playing
	bool IsFinished(ALuint sourceId);

	// Per-frame update (cleanup finished sources)
	void Update();

	// Get driver name
	const char *GetDriverName() const { return "OpenAL"; }

private:
	ALCdevice *m_device;
	ALCcontext *m_context;
	bool m_initialized;

	// Cached buffers: filename → buffer ID
	std::map<std::string, ALuint> m_bufferCache;

	// Active sources
	std::vector<ALuint> m_activeSources;

	// Parse WAV file header and return PCM data
	bool ParseWav(const char *filename, std::vector<uint8_t> &data,
	              int &channels, int &sampleRate, int &bitsPerSample);
};

} // namespace xrsound

#endif // !_WIN32
#endif // __OPENALBACKEND_H
