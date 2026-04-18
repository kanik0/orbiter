// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef _WIN32

#include "OGLJoystickCalibration.h"
#include "Orbiter.h"
#include "SDLPlatform.h"
#include "imgui.h"
#include <cstdio>
#include <cstdlib>

extern Orbiter *g_pOrbiter;

namespace ogl {

OGLJoystickCalibration::OGLJoystickCalibration()
	: ImGuiDialog("Joystick calibration", {520, 420})
{
}

void OGLJoystickCalibration::DrawAxis(const char *label, int rawValue)
{
	// SDLPlatform already maps each axis to [-1000, +1000] applying its
	// own static deadzone. We render the raw mapped value alongside the
	// configured deadzone so the user can see when their stick crosses
	// the threshold.
	int deadzone10k = g_pOrbiter->Cfg()->CfgJoystickPrm.Deadzone;
	float deadFrac  = (deadzone10k > 0)
		? (float)deadzone10k / 10000.0f * 1000.0f / 1000.0f
		: 0.0f; // half-width as fraction of 1.0
	float v = (float)rawValue / 1000.0f; // -1..+1

	ImGui::Text("%-8s %+5d", label, rawValue);
	ImVec2 sz(ImGui::GetContentRegionAvail().x - 8.0f, 14.0f);
	ImVec2 cur = ImGui::GetCursorScreenPos();
	ImDrawList *dl = ImGui::GetWindowDrawList();
	// Background bar
	dl->AddRectFilled(cur, ImVec2(cur.x + sz.x, cur.y + sz.y),
		IM_COL32(40, 40, 50, 255));
	// Deadzone band centred at midpoint
	float midX = cur.x + sz.x * 0.5f;
	float dzPx = sz.x * 0.5f * deadFrac;
	dl->AddRectFilled(ImVec2(midX - dzPx, cur.y),
		ImVec2(midX + dzPx, cur.y + sz.y),
		IM_COL32(80, 60, 30, 255));
	// Centre line
	dl->AddLine(ImVec2(midX, cur.y), ImVec2(midX, cur.y + sz.y),
		IM_COL32(180, 180, 180, 255));
	// Live needle
	float pxX = midX + sz.x * 0.5f * v;
	dl->AddRectFilled(ImVec2(pxX - 1.5f, cur.y - 2.0f),
		ImVec2(pxX + 1.5f, cur.y + sz.y + 2.0f),
		IM_COL32(80, 220, 120, 255));
	ImGui::Dummy(sz);
}

void OGLJoystickCalibration::DrawHat(int hat)
{
	const char *dir = "(centred)";
	if (hat != 0xFFFF) {
		if (hat <  4500 || hat > 31500) dir = "Up";
		else if (hat < 13500)           dir = "Right";
		else if (hat < 22500)           dir = "Down";
		else                             dir = "Left";
	}
	ImGui::Text("D-pad: %s (raw %u)", dir, (unsigned)hat);
}

void OGLJoystickCalibration::OnDraw()
{
	orbiter::SDLPlatform *sdl = g_pOrbiter ? g_pOrbiter->GetSDLPlatform() : nullptr;
	if (!sdl) {
		ImGui::TextDisabled("SDL platform layer not available.");
		return;
	}
	const auto &joy = sdl->GetJoyState();

	if (!joy.connected) {
		ImGui::TextWrapped("No game controller is connected. Plug in a joystick / pad and the live readout will appear automatically.");
	} else {
		ImGui::TextDisabled("Live SDL_GameController state — values mapped to [-1000, +1000]");
		ImGui::Separator();
		DrawAxis("L-stick X", joy.lX);
		DrawAxis("L-stick Y", joy.lY);
		DrawAxis("R-stick X", joy.lRx);
		DrawAxis("R-stick Y", joy.lRy);
		DrawAxis("L trigger", joy.lZ);
		DrawAxis("R trigger", joy.lRz);
		DrawHat(joy.hatAngle);
		ImGui::Separator();
		ImGui::Text("Buttons:");
		for (int i = 0; i < 16; ++i) {
			ImGui::SameLine();
			ImGui::TextColored(joy.buttons[i]
				? ImVec4(0.4f, 1.0f, 0.4f, 1.0f)
				: ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%d", i);
		}
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::TextDisabled("Calibration");

	auto &p = g_pOrbiter->Cfg()->CfgJoystickPrm;
	ImGui::SliderInt("Central deadzone (0-10000)", &p.Deadzone, 0, 10000);
	ImGui::SliderInt("Throttle saturation (0-10000)",
		&p.ThrottleSaturation, 0, 10000);
	ImGui::Checkbox("Ignore initial throttle position",
		&p.bThrottleIgnore);

	ImGui::Spacing();
	ImGui::TextWrapped("Deadzone is shown as the dim band around centre — "
		"axis input within that band is suppressed by SDLPlatform. "
		"Changes take effect immediately and are persisted via the "
		"main Config save (in-sim Options dialog → Save).");
}

void OpenJoystickCalibration()
{
	if (!g_pOrbiter || !g_pOrbiter->DlgMgr()) return;
	g_pOrbiter->DlgMgr()->EnsureEntry<OGLJoystickCalibration>();
}

} // namespace ogl

#endif // !_WIN32
