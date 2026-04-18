// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#include "HapticFX.h"

#ifndef _WIN32
#include <SDL_gamecontroller.h>
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
	m_buffetLevel = 0.0f;
	m_transientLow = m_transientHigh = 0.0f;
	m_transientLeft = 0.0;
}

static inline Uint16 ScaleAmp(float v01)
{
	if (v01 < 0.0f) v01 = 0.0f;
	if (v01 > 1.0f) v01 = 1.0f;
	return (Uint16)std::lround(v01 * 65535.0f);
}

void HapticFX::Tick(double simdt)
{
	if (!m_gc) return;

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
	// with a saturating transient.
	float low  = std::min(1.0f, m_buffetLevel + m_transientLow);
	float high = std::min(1.0f, m_transientHigh);

	// Reapply every frame so the buffet keeps running for as long as
	// the caller asserts a non-zero level. We use a 200 ms duration
	// so a missed frame at 60 fps still leaves us with 12 buffer
	// frames before silence.
	SDL_GameControllerRumble(m_gc, ScaleAmp(low), ScaleAmp(high), 200);
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
void HapticFX::Tick(double)                       {}
void HapticFX::Touchdown(float)                  {}
void HapticFX::EngineIgnite(float)               {}
void HapticFX::AtmosphericBuffet(float)          {}
void HapticFX::Stop()                             {}
} // namespace orbiter

#endif // _WIN32
