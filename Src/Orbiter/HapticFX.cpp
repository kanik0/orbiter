// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#include "HapticFX.h"

#ifndef _WIN32
#include <SDL_gamecontroller.h>
#include <SDL_haptic.h>
#include <algorithm>
#include <cmath>

namespace orbiter {

HapticFX::HapticFX() = default;

HapticFX::~HapticFX()
{
	Stop();
}

void HapticFX::Bind(SDL_GameController *gc)
{
	if (m_gc == gc) return;
	if (m_gc) {
		SDL_GameControllerRumble(m_gc, 0, 0, 0); // stop on detach
	}
	m_gc = gc;
	// The GameController path supersedes the legacy Haptic fallback —
	// drop any pending fallback binding when a GC becomes available.
	if (m_gc && m_haptic) m_haptic = nullptr;
	m_buffetLevel = 0.0f;
	m_transientLow = m_transientHigh = 0.0f;
	m_transientLeft = 0.0;
}

void HapticFX::BindHaptic(SDL_Haptic *haptic)
{
	// Only use the legacy fallback when there's no GameController; the
	// GC path is strictly richer (two independent motors, no need for
	// the caller to SDL_HapticRumbleInit first).
	if (m_gc) return;
	if (m_haptic == haptic) return;
	if (m_haptic) SDL_HapticRumbleStop(m_haptic);
	m_haptic = haptic;
	m_buffetLevel = 0.0f;
	m_transientLow = m_transientHigh = 0.0f;
	m_transientLeft = 0.0;
}

void HapticFX::SetGain(float gain)
{
	if (gain < 0.0f) gain = 0.0f;
	if (gain > 2.0f) gain = 2.0f;
	m_gain = gain;
}

static inline Uint16 ScaleAmp(float v01)
{
	if (v01 < 0.0f) v01 = 0.0f;
	if (v01 > 1.0f) v01 = 1.0f;
	return (Uint16)std::lround(v01 * 65535.0f);
}

void HapticFX::Tick(double simdt)
{
	if (!m_gc && !m_haptic) return;

	// Decay the transient envelope.
	if (m_transientLeft > 0.0) {
		m_transientLeft -= simdt;
		if (m_transientLeft <= 0.0) {
			m_transientLeft = 0.0;
			m_transientLow = m_transientHigh = 0.0f;
		}
	}

	// Mix the channels — buffeting is low frequency, transients ride
	// on top with the higher-frequency motor. Clamp to 1.0 so the
	// SDL conversion stays linear and we don't smother the buffet
	// with a saturating transient. The master gain scales both
	// motors equally; 0 disables rumble without disconnecting.
	float low  = std::min(1.0f, (m_buffetLevel + m_transientLow) * m_gain);
	float high = std::min(1.0f, m_transientHigh * m_gain);

	if (m_gc) {
		// Reapply every frame so the buffet keeps running for as
		// long as the caller asserts a non-zero level. We use a
		// 200 ms duration so a missed frame at 60 fps still leaves
		// us with 12 buffer frames before silence.
		SDL_GameControllerRumble(m_gc, ScaleAmp(low), ScaleAmp(high), 200);
	} else {
		// Legacy SDL_Haptic path: SDL_HapticRumblePlay takes a single
		// magnitude in [0..1] and a duration. Collapse the two motors
		// by taking the larger amplitude so transients stay audible
		// over a sustained buffet.
		float mag = std::max(low, high);
		SDL_HapticRumblePlay(m_haptic, mag, 200);
	}
}

void HapticFX::Touchdown(float intensity)
{
	// 250 ms heavy bump biased toward the low motor — feels like a
	// gear strut compression rather than a sharp hit.
	m_transientLow  = std::max(m_transientLow,  intensity * 0.85f);
	m_transientHigh = std::max(m_transientHigh, intensity * 0.40f);
	m_transientLeft = std::max(m_transientLeft, 0.25);
}

void HapticFX::EngineIgnite(float intensity)
{
	// 120 ms shorter, brighter pulse — main engine spool / kick.
	m_transientLow  = std::max(m_transientLow,  intensity * 0.45f);
	m_transientHigh = std::max(m_transientHigh, intensity * 0.80f);
	m_transientLeft = std::max(m_transientLeft, 0.12);
}

void HapticFX::AtmosphericBuffet(float intensity)
{
	if (intensity < 0.0f) intensity = 0.0f;
	if (intensity > 1.0f) intensity = 1.0f;
	m_buffetLevel = intensity;
}

void HapticFX::Stop()
{
	if (m_gc) SDL_GameControllerRumble(m_gc, 0, 0, 0);
	if (m_haptic) SDL_HapticRumbleStop(m_haptic);
	m_buffetLevel = 0.0f;
	m_transientLow = m_transientHigh = 0.0f;
	m_transientLeft = 0.0;
}

} // namespace orbiter

#else // _WIN32

// Win32 stubs — DirectInput already exposes IDirectInputDevice8::
// SetEffect on capable joysticks; HapticFX is a macOS / Linux
// addition. The empty implementation keeps the header callable from
// cross-platform code without a `#ifdef` cascade.

namespace orbiter {
HapticFX::HapticFX() = default;
HapticFX::~HapticFX() = default;
void HapticFX::Bind(_SDL_GameController*)        {}
void HapticFX::BindHaptic(_SDL_Haptic*)          {}
void HapticFX::SetGain(float)                    {}
void HapticFX::Tick(double)                       {}
void HapticFX::Touchdown(float)                  {}
void HapticFX::EngineIgnite(float)               {}
void HapticFX::AtmosphericBuffet(float)          {}
void HapticFX::Stop()                             {}
} // namespace orbiter

#endif // _WIN32
