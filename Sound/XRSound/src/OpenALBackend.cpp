// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#if !defined(_WIN32) || !defined(XRSOUND_DLL_BUILD)

#include "OpenALBackend.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>

namespace xrsound {

// ============================================================================
// OpenALSound
// ============================================================================

OpenALSound::OpenALSound(ALuint source, ALuint buffer, float durationSeconds)
	: m_source(source), m_buffer(buffer), m_durationSec(durationSeconds),
	  m_pan(0.0f), m_volume(1.0f)
{
}

OpenALSound::~OpenALSound()
{
	if (m_source) {
		alSourceStop(m_source);
		alDeleteSources(1, &m_source);
	}
}

void OpenALSound::drop() { delete this; }

void OpenALSound::stop()
{
	if (m_source) alSourceStop(m_source);
}

bool OpenALSound::isFinished()
{
	if (!m_source) return true;
	ALint state = 0;
	alGetSourcei(m_source, AL_SOURCE_STATE, &state);
	return state == AL_STOPPED || state == AL_INITIAL;
}

void OpenALSound::setVolume(float volume)
{
	if (!m_source) return;
	m_volume = volume;
	alSourcef(m_source, AL_GAIN, volume);
}

void OpenALSound::setIsLooped(bool loop)
{
	if (!m_source) return;
	alSourcei(m_source, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
}

void OpenALSound::setIsPaused(bool pause)
{
	if (!m_source) return;
	if (pause) alSourcePause(m_source);
	else       alSourcePlay(m_source);
}

// Pan is simulated by positioning the source slightly off the listener's
// x-axis. Audible match to irrKlang's 2D pan is close enough for cockpit
// sound effects; precise HRTF would require OpenAL EFX setup which adds
// no value for the MFD/cockpit soundscape XRSound produces.
void OpenALSound::setPan(float pan)
{
	if (!m_source) return;
	m_pan = std::max(-1.0f, std::min(1.0f, pan));
	alSource3f(m_source, AL_POSITION, m_pan, 0.0f, -std::sqrt(std::max(0.0f, 1.0f - m_pan * m_pan)));
	alSourcei (m_source, AL_SOURCE_RELATIVE, AL_TRUE);
}

float OpenALSound::getPan() { return m_pan; }

bool OpenALSound::setPlaybackSpeed(float speed)
{
	if (!m_source || speed <= 0.0f) return false;
	alSourcef(m_source, AL_PITCH, speed);
	return alGetError() == AL_NO_ERROR;
}

float OpenALSound::getPlaybackSpeed()
{
	if (!m_source) return 1.0f;
	ALfloat p = 1.0f;
	alGetSourcef(m_source, AL_PITCH, &p);
	return p;
}

bool OpenALSound::setPlayPosition(unsigned int positionMs)
{
	if (!m_source) return false;
	alSourcef(m_source, AL_SEC_OFFSET, positionMs * 0.001f);
	return alGetError() == AL_NO_ERROR;
}

unsigned int OpenALSound::getPlayPosition()
{
	if (!m_source) return 0;
	ALfloat sec = 0.0f;
	alGetSourcef(m_source, AL_SEC_OFFSET, &sec);
	return (unsigned int)(sec * 1000.0f);
}

// ============================================================================
// OpenALEngine
// ============================================================================

OpenALEngine::OpenALEngine()
	: m_device(nullptr), m_context(nullptr), m_initialized(false),
	  m_driverName("OpenAL")
{
}

OpenALEngine::~OpenALEngine()
{
	drop();
}

bool OpenALEngine::init()
{
	if (m_initialized) return true;

	m_device = alcOpenDevice(nullptr);
	if (!m_device) {
		fprintf(stderr, "[OpenAL] alcOpenDevice failed\n");
		return false;
	}
	m_context = alcCreateContext(m_device, nullptr);
	if (!m_context) {
		fprintf(stderr, "[OpenAL] alcCreateContext failed\n");
		alcCloseDevice(m_device);
		m_device = nullptr;
		return false;
	}
	alcMakeContextCurrent(m_context);

	// Compose a friendly driver name for log output + Orbiter UI.
	const char *spec = alcGetString(m_device, ALC_DEVICE_SPECIFIER);
	const char *vendor = alGetString(AL_VENDOR);
	char buf[256];
	std::snprintf(buf, sizeof(buf), "OpenAL (%s / %s)",
	              vendor ? vendor : "unknown",
	              spec   ? spec   : "default");
	m_driverName = buf;
	fprintf(stderr, "[OpenAL] %s\n", m_driverName.c_str());

	m_initialized = true;
	return true;
}

void OpenALEngine::drop()
{
	if (!m_initialized) return;

	for (auto &kv : m_bufferCache) alDeleteBuffers(1, &kv.second);
	m_bufferCache.clear();

	alcMakeContextCurrent(nullptr);
	if (m_context) { alcDestroyContext(m_context); m_context = nullptr; }
	if (m_device)  { alcCloseDevice(m_device);     m_device  = nullptr; }
	m_initialized = false;
}

const char *OpenALEngine::getDriverName() { return m_driverName.c_str(); }

void OpenALEngine::update() { /* OpenAL has no per-frame work */ }

bool OpenALEngine::ParseWav(const char *filename, std::vector<uint8_t> &data,
                            int &channels, int &sampleRate, int &bitsPerSample)
{
	std::ifstream file(filename, std::ios::binary);
	if (!file.is_open()) return false;

	char riff[4];
	file.read(riff, 4);
	if (std::memcmp(riff, "RIFF", 4) != 0) return false;

	uint32_t fileSize;
	file.read((char*)&fileSize, 4);

	char wave[4];
	file.read(wave, 4);
	if (std::memcmp(wave, "WAVE", 4) != 0) return false;

	bool foundFmt = false, foundData = false;
	while (file && !file.eof()) {
		char chunkId[4];
		uint32_t chunkSize;
		file.read(chunkId, 4);
		if (file.gcount() < 4) break;
		file.read((char*)&chunkSize, 4);

		if (std::memcmp(chunkId, "fmt ", 4) == 0) {
			uint16_t audioFormat, numChannels;
			uint32_t sr, byteRate;
			uint16_t blockAlign, bps;
			file.read((char*)&audioFormat, 2);
			file.read((char*)&numChannels, 2);
			file.read((char*)&sr, 4);
			file.read((char*)&byteRate, 4);
			file.read((char*)&blockAlign, 2);
			file.read((char*)&bps, 2);
			channels      = numChannels;
			sampleRate    = (int)sr;
			bitsPerSample = bps;
			if (chunkSize > 16)
				file.seekg(chunkSize - 16, std::ios::cur);
			foundFmt = true;
		} else if (std::memcmp(chunkId, "data", 4) == 0) {
			data.resize(chunkSize);
			file.read((char*)data.data(), chunkSize);
			foundData = true;
			break;
		} else {
			file.seekg(chunkSize, std::ios::cur);
		}
	}

	return foundFmt && foundData && !data.empty();
}

ALuint OpenALEngine::LoadFile(const char *filename, float *outDurationSec)
{
	if (!m_initialized || !filename) return 0;

	auto it = m_bufferCache.find(filename);
	if (it != m_bufferCache.end()) {
		// Duration is recomputed cheaply from buffer metadata below.
		ALint size = 0, freq = 0, bits = 0, chans = 0;
		alGetBufferi(it->second, AL_SIZE, &size);
		alGetBufferi(it->second, AL_FREQUENCY, &freq);
		alGetBufferi(it->second, AL_BITS, &bits);
		alGetBufferi(it->second, AL_CHANNELS, &chans);
		if (outDurationSec && freq > 0 && bits > 0 && chans > 0) {
			*outDurationSec = (float)size / (float)(freq * (bits / 8) * chans);
		}
		return it->second;
	}

	std::vector<uint8_t> data;
	int channels = 0, sampleRate = 0, bitsPerSample = 0;
	if (!ParseWav(filename, data, channels, sampleRate, bitsPerSample)) {
		// OGG / MP3 decoders will hook in here as part of M18.b; until
		// then we report the unsupported format but do not assert so
		// the rest of the soundscape still plays.
		fprintf(stderr, "[OpenAL] Unsupported / missing file '%s'\n", filename);
		return 0;
	}

	ALenum format = 0;
	if      (channels == 1 && bitsPerSample ==  8) format = AL_FORMAT_MONO8;
	else if (channels == 1 && bitsPerSample == 16) format = AL_FORMAT_MONO16;
	else if (channels == 2 && bitsPerSample ==  8) format = AL_FORMAT_STEREO8;
	else if (channels == 2 && bitsPerSample == 16) format = AL_FORMAT_STEREO16;
	else {
		fprintf(stderr, "[OpenAL] Unsupported WAV format: %d ch %d bps in '%s'\n",
		        channels, bitsPerSample, filename);
		return 0;
	}

	ALuint bufferId = 0;
	alGenBuffers(1, &bufferId);
	alBufferData(bufferId, format, data.data(), (ALsizei)data.size(), sampleRate);

	ALenum err = alGetError();
	if (err != AL_NO_ERROR) {
		fprintf(stderr, "[OpenAL] alBufferData error 0x%x for '%s'\n", err, filename);
		alDeleteBuffers(1, &bufferId);
		return 0;
	}

	if (outDurationSec) {
		*outDurationSec = (float)data.size() /
		                  (float)(sampleRate * (bitsPerSample / 8) * channels);
	}
	m_bufferCache[filename] = bufferId;
	return bufferId;
}

IAudioSound *OpenALEngine::play2D(const char *filename, bool loop,
                                  bool startPaused, bool /*track*/)
{
	if (!m_initialized || !filename) return nullptr;

	float duration = 0.0f;
	ALuint buffer = LoadFile(filename, &duration);
	if (!buffer) return nullptr;

	ALuint source = 0;
	alGenSources(1, &source);
	alSourcei (source, AL_BUFFER,  buffer);
	alSourcei (source, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
	alSourcef (source, AL_GAIN,    1.0f);
	alSourcef (source, AL_PITCH,   1.0f);
	alSourcei (source, AL_SOURCE_RELATIVE, AL_TRUE);
	alSource3f(source, AL_POSITION, 0.0f, 0.0f, -1.0f);
	if (!startPaused) alSourcePlay(source);

	return new OpenALSound(source, buffer, duration);
}

// ============================================================================
// Factory
// ============================================================================

IAudioBackend *createAudioBackend()
{
	OpenALEngine *e = new OpenALEngine();
	if (!e->init()) {
		delete e;
		return nullptr;
	}
	return e;
}

} // namespace xrsound

#endif // !_WIN32 || !XRSOUND_DLL_BUILD
