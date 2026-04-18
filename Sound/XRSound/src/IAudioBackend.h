// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// IAudioBackend — platform-agnostic audio engine interface for XRSound.
//
// On Windows the interface is a direct typedef onto irrKlang's
// ISoundEngine / ISound, so no existing caller changes: the whole
// XRSoundEngine.cpp compiles unchanged against the production backend.
//
// On macOS/Linux the interface is an abstract base class with the same
// method signatures. OpenALBackend.cpp supplies the concrete
// implementation (see OpenALEngine + OpenALSound).
//
// The method set mirrors every irrKlang member actually invoked by
// XRSoundEngine* — a targeted audit (see the grep in commit log) found
// exactly 14 methods, listed below.

#ifndef __IAUDIOBACKEND_H
#define __IAUDIOBACKEND_H

#if defined(_WIN32) && defined(XRSOUND_DLL_BUILD)

// -------------------------------------------------------------------------
// Windows DLL build: keep irrKlang as the production backend, expose its
// types directly so every pISound/pSoundEngine callsite compiles unchanged.
// -------------------------------------------------------------------------

#include <irrKlang.h>

namespace xrsound {
	typedef irrklang::ISoundEngine IAudioBackend;
	typedef irrklang::ISound       IAudioSound;
}

#else

// -------------------------------------------------------------------------
// Non-Windows (or lib client): abstract interfaces with irrKlang-matching
// method names. The concrete implementation lives in OpenALBackend.cpp.
// -------------------------------------------------------------------------

namespace xrsound {

// Per-source playback handle. Returned by IAudioBackend::play2D, mirrors
// the subset of irrklang::ISound that XRSoundEngine actually touches.
class IAudioSound {
public:
	virtual ~IAudioSound() = default;

	// Explicit reference decrement — the irrKlang API treats ISound as
	// reference-counted; XRSoundEngine calls drop() once per stop/abort.
	// OpenALBackend implements it as "delete this" because we don't
	// share source objects between callers.
	virtual void drop() = 0;

	virtual void stop() = 0;
	virtual bool isFinished() = 0;

	virtual void setVolume(float volume) = 0;
	virtual void setIsLooped(bool loop) = 0;
	virtual void setIsPaused(bool pause) = 0;

	// Pan is -1..+1; 0 is centre.
	virtual void setPan(float pan) = 0;
	virtual float getPan() = 0;

	// Playback speed (pitch) as a multiplier of the recorded rate.
	virtual bool  setPlaybackSpeed(float speed) = 0;
	virtual float getPlaybackSpeed() = 0;

	// Position in milliseconds. Used for seeking and progress reporting.
	virtual bool         setPlayPosition(unsigned int positionMs) = 0;
	virtual unsigned int getPlayPosition() = 0;
};

class IAudioBackend {
public:
	virtual ~IAudioBackend() = default;

	// Matches irrklang::ISoundEngine::drop() — release the engine and
	// everything it owns. Concrete backends can forward to `delete this`.
	virtual void drop() = 0;

	// Fire-and-forget playback. `track` is kept for irrKlang API parity;
	// the OpenAL path always returns a tracked IAudioSound so the caller
	// can query isFinished / tweak volume / stop.
	virtual IAudioSound *play2D(const char *filename,
	                            bool playLooped      = false,
	                            bool startPaused     = false,
	                            bool track           = false) = 0;

	// Called once per frame; OpenAL uses it to GC finished non-tracked
	// sources, irrKlang has its own thread loop so the call is a no-op
	// for the IrrklangBackend.
	virtual void update() = 0;

	virtual const char *getDriverName() = 0;
};

// Factory — returns a backend pointer appropriate for the running
// platform. The caller owns the pointer and must release it via drop().
IAudioBackend *createAudioBackend();

} // namespace xrsound

#endif // !_WIN32 || !XRSOUND_DLL_BUILD

#endif // __IAUDIOBACKEND_H
