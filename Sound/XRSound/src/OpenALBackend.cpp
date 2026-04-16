// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// OpenAL audio backend implementation

#ifndef _WIN32

#include "OpenALBackend.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <algorithm>

namespace xrsound {

OpenALEngine::OpenALEngine()
	: m_device(nullptr), m_context(nullptr), m_initialized(false)
{
}

OpenALEngine::~OpenALEngine()
{
	Shutdown();
}

bool OpenALEngine::Init()
{
	if (m_initialized) return true;

	m_device = alcOpenDevice(nullptr); // default device
	if (!m_device) {
		fprintf(stderr, "[OpenAL] Failed to open audio device\n");
		return false;
	}

	m_context = alcCreateContext(m_device, nullptr);
	if (!m_context) {
		fprintf(stderr, "[OpenAL] Failed to create context\n");
		alcCloseDevice(m_device);
		m_device = nullptr;
		return false;
	}

	alcMakeContextCurrent(m_context);
	m_initialized = true;

	const char *vendor = alGetString(AL_VENDOR);
	const char *renderer = alGetString(AL_RENDERER);
	fprintf(stderr, "[OpenAL] Initialized: %s / %s\n",
		vendor ? vendor : "unknown", renderer ? renderer : "unknown");

	return true;
}

void OpenALEngine::Shutdown()
{
	if (!m_initialized) return;

	// Stop and delete all sources
	for (ALuint src : m_activeSources) {
		alSourceStop(src);
		alDeleteSources(1, &src);
	}
	m_activeSources.clear();

	// Delete all buffers
	for (auto &kv : m_bufferCache) {
		alDeleteBuffers(1, &kv.second);
	}
	m_bufferCache.clear();

	alcMakeContextCurrent(nullptr);
	if (m_context) { alcDestroyContext(m_context); m_context = nullptr; }
	if (m_device) { alcCloseDevice(m_device); m_device = nullptr; }

	m_initialized = false;
	fprintf(stderr, "[OpenAL] Shutdown\n");
}

bool OpenALEngine::ParseWav(const char *filename, std::vector<uint8_t> &data,
                             int &channels, int &sampleRate, int &bitsPerSample)
{
	std::ifstream file(filename, std::ios::binary);
	if (!file.is_open()) return false;

	// RIFF header
	char riff[4];
	file.read(riff, 4);
	if (memcmp(riff, "RIFF", 4) != 0) return false;

	uint32_t fileSize;
	file.read((char*)&fileSize, 4);

	char wave[4];
	file.read(wave, 4);
	if (memcmp(wave, "WAVE", 4) != 0) return false;

	// Find fmt and data chunks
	bool foundFmt = false, foundData = false;
	while (!file.eof()) {
		char chunkId[4];
		uint32_t chunkSize;
		file.read(chunkId, 4);
		file.read((char*)&chunkSize, 4);
		if (file.eof()) break;

		if (memcmp(chunkId, "fmt ", 4) == 0) {
			uint16_t audioFormat, numChannels;
			uint32_t sr, byteRate;
			uint16_t blockAlign, bps;

			file.read((char*)&audioFormat, 2);
			file.read((char*)&numChannels, 2);
			file.read((char*)&sr, 4);
			file.read((char*)&byteRate, 4);
			file.read((char*)&blockAlign, 2);
			file.read((char*)&bps, 2);

			channels = numChannels;
			sampleRate = sr;
			bitsPerSample = bps;

			// Skip any extra fmt bytes
			if (chunkSize > 16)
				file.seekg(chunkSize - 16, std::ios::cur);

			foundFmt = true;
		} else if (memcmp(chunkId, "data", 4) == 0) {
			data.resize(chunkSize);
			file.read((char*)data.data(), chunkSize);
			foundData = true;
			break;
		} else {
			// Skip unknown chunk
			file.seekg(chunkSize, std::ios::cur);
		}
	}

	return foundFmt && foundData && !data.empty();
}

ALuint OpenALEngine::LoadWav(const char *filename)
{
	if (!m_initialized || !filename) return 0;

	// Check cache
	auto it = m_bufferCache.find(filename);
	if (it != m_bufferCache.end()) return it->second;

	// Parse WAV
	std::vector<uint8_t> data;
	int channels, sampleRate, bitsPerSample;
	if (!ParseWav(filename, data, channels, sampleRate, bitsPerSample)) {
		return 0;
	}

	// Determine OpenAL format
	ALenum format;
	if (channels == 1 && bitsPerSample == 8) format = AL_FORMAT_MONO8;
	else if (channels == 1 && bitsPerSample == 16) format = AL_FORMAT_MONO16;
	else if (channels == 2 && bitsPerSample == 8) format = AL_FORMAT_STEREO8;
	else if (channels == 2 && bitsPerSample == 16) format = AL_FORMAT_STEREO16;
	else {
		fprintf(stderr, "[OpenAL] Unsupported format: %d ch, %d bps in '%s'\n",
			channels, bitsPerSample, filename);
		return 0;
	}

	// Create buffer
	ALuint bufferId;
	alGenBuffers(1, &bufferId);
	alBufferData(bufferId, format, data.data(), (ALsizei)data.size(), sampleRate);

	ALenum err = alGetError();
	if (err != AL_NO_ERROR) {
		fprintf(stderr, "[OpenAL] Buffer error 0x%x for '%s'\n", err, filename);
		alDeleteBuffers(1, &bufferId);
		return 0;
	}

	m_bufferCache[filename] = bufferId;
	return bufferId;
}

ALuint OpenALEngine::Play(ALuint bufferId, bool loop, float volume)
{
	if (!m_initialized || !bufferId) return 0;

	ALuint sourceId;
	alGenSources(1, &sourceId);
	alSourcei(sourceId, AL_BUFFER, bufferId);
	alSourcei(sourceId, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
	alSourcef(sourceId, AL_GAIN, volume);
	alSourcePlay(sourceId);

	m_activeSources.push_back(sourceId);
	return sourceId;
}

void OpenALEngine::Stop(ALuint sourceId)
{
	if (!sourceId) return;
	alSourceStop(sourceId);
	alDeleteSources(1, &sourceId);
	m_activeSources.erase(
		std::remove(m_activeSources.begin(), m_activeSources.end(), sourceId),
		m_activeSources.end());
}

void OpenALEngine::SetVolume(ALuint sourceId, float volume)
{
	if (sourceId) alSourcef(sourceId, AL_GAIN, volume);
}

void OpenALEngine::Pause(ALuint sourceId)
{
	if (sourceId) alSourcePause(sourceId);
}

void OpenALEngine::Resume(ALuint sourceId)
{
	if (sourceId) alSourcePlay(sourceId);
}

bool OpenALEngine::IsFinished(ALuint sourceId)
{
	if (!sourceId) return true;
	ALint state;
	alGetSourcei(sourceId, AL_SOURCE_STATE, &state);
	return state == AL_STOPPED;
}

void OpenALEngine::Update()
{
	// Cleanup finished non-looping sources
	for (auto it = m_activeSources.begin(); it != m_activeSources.end(); ) {
		ALint state;
		alGetSourcei(*it, AL_SOURCE_STATE, &state);
		if (state == AL_STOPPED) {
			alDeleteSources(1, &*it);
			it = m_activeSources.erase(it);
		} else {
			++it;
		}
	}
}

} // namespace xrsound

#endif // !_WIN32
