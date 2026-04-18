// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// OGLJoystickCalibration — live joystick axis monitor and deadzone /
// throttle saturation tuning UI for the Orbiter macOS / Linux port.
//
// Reads the live SDL_GameController state from SDLPlatform every
// frame, draws horizontal axis bars with the configured deadzone
// overlaid, and lets the user slide the deadzone (0..10000) and the
// throttle saturation (0..10000) into CfgJoystickPrm. Saving is
// implicit via the master Config write (Apply on Launch from the
// in-sim Options dialog or via Save in the editor's footer).

#ifndef __OGL_JOYSTICK_CALIBRATION_H
#define __OGL_JOYSTICK_CALIBRATION_H

#ifndef _WIN32

#include "DlgMgr.h"

namespace ogl {

class OGLJoystickCalibration : public ImGuiDialog
{
public:
	OGLJoystickCalibration();
	void OnDraw() override;

private:
	void DrawAxis(const char *label, int rawValue);
	void DrawHat(int hat);
};

void OpenJoystickCalibration();

} // namespace ogl

#endif // !_WIN32
#endif // __OGL_JOYSTICK_CALIBRATION_H
