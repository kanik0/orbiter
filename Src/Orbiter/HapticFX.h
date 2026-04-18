// Copyright (c) Martin Schweiger
// Licensed under the MIT License
//
// HapticFX — gamepad rumble / force-feedback for the Orbiter macOS /
// Linux port.
//
// Uses SDL2's SDL_GameControllerRumble API on top of the SDL game
// controller already opened by SDLPlatform. The Win32 build doesn't
// need this layer (DirectInput already exposes IDirectInputDevice8
// SetEffect on capable joysticks); the header still compiles
// everywhere so callers can stay free of `#ifdef`.
//
// Effect channel summary (intensity is a 0..1 scalar; durations in
// milliseconds — anything < 16 ms gets clamped to one frame):
//
//   * Touchdown(intensity)         — short bump on the rising edge of
//                                    Vesselbase::bSurfaceContact when
//                                    descent rate was non-trivial.
//   * AtmosphericBuffet(intensity) — sustained low-frequency rumble
//                                    while dynamic pressure exceeds
//                                    a threshold (re-entry, dense
//                                    atmosphere, spinning at speed).
//   * EngineIgnite(intensity)      — short medium-frequency bump
//                                    when main thrust transitions
//                                    from idle to positive.
//   * Stop()                        — abort all running effects (used
//                                    on session end / pause).

#ifndef __HAPTICFX_H
#define __HAPTICFX_H

#include <cstdint>

struct _SDL_GameController;

namespace orbiter {

class HapticFX
{
public:
	HapticFX();
	~HapticFX();

	// Bind to the SDL_GameController owned by SDLPlatform. nullptr
	// detaches and stops any running effect. Idempotent.
	void Bind(_SDL_GameController *gc);

	// Per-frame tick: reapplies the sustained AtmosphericBuffet
	// envelope (SDL_GameControllerRumble durations are bounded, so
	// long-running effects need to be refreshed) and decays
	// transient effects.
	void Tick(double simdt);

	// Triggered effects (intensity 0..1, duration in milliseconds
	// for finite-length effects):
	void Touchdown(float intensity);
	void EngineIgnite(float intensity);
	void AtmosphericBuffet(float intensity); // 0 stops the channel
	void Stop();

	bool IsAvailable() const { return m_gc != nullptr; }

private:
	_SDL_GameController *m_gc = nullptr;

	// Mixed-channel state. Each channel contributes a low-freq + a
	// high-freq amplitude; the per-frame tick combines them and
	// pushes the result through SDL_GameControllerRumble.
	float m_buffetLevel = 0.0f;
	float m_transientLow = 0.0f;
	float m_transientHigh = 0.0f;
	double m_transientLeft = 0.0; // seconds remaining
};

} // namespace orbiter

#endif // !__HAPTICFX_H
