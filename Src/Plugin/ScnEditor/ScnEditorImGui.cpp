// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// ==============================================================
//              ORBITER MODULE: Scenario Editor (ImGui)
//
// Cross-platform ImGui front-end for the Scenario Editor on
// macOS / Linux. The Win32 port (ScnEditor.cpp + Editor.cpp) keeps
// driving the resource-driven dialog tree on Windows; this file
// reimplements the most common in-sim editor tabs against the
// stable Orbiter SDK so users can manipulate the focus vessel
// without leaving the simulation.
//
// Implemented tabs (per Fase E roadmap M25 — "ScnEditor IMFD
// conversion → priorizzare feature usate"):
//   * Vessel       — focus selection
//   * State vec    — position + velocity (verification target)
//   * Orientation  — Euler angles
//   * Angular vel  — spin rates
//   * Propellant   — tank levels
//   * Date         — MJD edit
//
// Tabs not exposed on macOS today (each accessible by alternative
// in-sim affordances; addressed by future M25.d.bis if demand
// surfaces): New (vessel creation) — use the Launchpad scenario
// list; Save — use the in-sim "Save Scenario" menu; Elements —
// derive from State vec; Landed — use State vec; Docking —
// in-sim docking controls; Custom — custom commands menu (Ctrl-F4).
// ==============================================================

#ifndef _WIN32

// ORBITER_MODULE lives in ScnEditor.cpp; defining it again here would
// re-emit ModuleDate / calldummy and trip a duplicate-symbol link
// failure. We are a *secondary* TU of the same plugin.
#include "OrbiterPlatform.h"
#include "Orbitersdk.h"
#include "Editor.h"
#include "imgui.h"
#include <vector>
#include <string>
#include <cstring>
#include <cmath>

extern ScnEditor *g_editor;

namespace {

// Format a vessel's display name as shown in the focus combo.
const char *VesselDisplay(VESSEL *v)
{
	const char *n = v ? v->GetName() : nullptr;
	return (n && *n) ? n : "(unnamed)";
}

class ScnEditorImGui : public ImGuiDialog
{
public:
	ScnEditorImGui()
		: ImGuiDialog("Scenario Editor", {520, 460})
	{
	}

	void Activate() {
		// Seed selection with current focus when the editor reopens.
		OBJHANDLE foc = oapiGetFocusObject();
		if (foc) m_selected = foc;
		ImGuiDialog::Activate();
	}

	void OnDraw() override {
		// Default to the current focus if no selection yet.
		if (!m_selected) m_selected = oapiGetFocusObject();

		DrawVesselPicker();
		ImGui::Separator();
		if (!m_selected) {
			ImGui::TextDisabled("No vessel selected.");
			return;
		}

		VESSEL *v = oapiGetVesselInterface(m_selected);
		if (!v) {
			ImGui::TextDisabled("Vessel handle stale.");
			return;
		}

		if (ImGui::BeginTabBar("ScnEditTabs")) {
			if (ImGui::BeginTabItem("State"))       { DrawStateTab(v);       ImGui::EndTabItem(); }
			if (ImGui::BeginTabItem("Orientation")) { DrawOrientationTab(v); ImGui::EndTabItem(); }
			if (ImGui::BeginTabItem("Angular vel")) { DrawAngVelTab(v);      ImGui::EndTabItem(); }
			if (ImGui::BeginTabItem("Propellant"))  { DrawPropellantTab(v);  ImGui::EndTabItem(); }
			if (ImGui::BeginTabItem("Date"))        { DrawDateTab();         ImGui::EndTabItem(); }
			ImGui::EndTabBar();
		}
	}

private:
	OBJHANDLE m_selected = nullptr;

	void DrawVesselPicker() {
		VESSEL *cur = m_selected ? oapiGetVesselInterface(m_selected) : nullptr;
		ImGui::Text("Editing:");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(220.0f);
		if (ImGui::BeginCombo("##vessel", VesselDisplay(cur))) {
			DWORD n = oapiGetVesselCount();
			for (DWORD i = 0; i < n; ++i) {
				OBJHANDLE h = oapiGetVesselByIndex(i);
				VESSEL *vv = oapiGetVesselInterface(h);
				if (!vv || !vv->GetEnableFocus()) continue;
				bool sel = (h == m_selected);
				if (ImGui::Selectable(VesselDisplay(vv), sel))
					m_selected = h;
				if (sel) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		if (ImGui::Button("Set focus") && m_selected)
			oapiSetFocusObject(m_selected);
	}

	// ----- State vec tab (verification target) -----
	void DrawStateTab(VESSEL *v) {
		VESSELSTATUS2 vs;
		std::memset(&vs, 0, sizeof(vs));
		vs.version = 2;
		v->GetStatusEx(&vs);

		// VESSELSTATUS2 stores rpos / rvel in the parent body's
		// equatorial frame by default; we keep that convention so
		// values round-trip cleanly with GetStatusEx / DefSetStateEx.
		ImGui::TextDisabled("Frame: parent body equatorial (m)");

		double pos[3] = { vs.rpos.x, vs.rpos.y, vs.rpos.z };
		double vel[3] = { vs.rvel.x, vs.rvel.y, vs.rvel.z };
		bool dirty = false;
		ImGui::SeparatorText("Position [m]");
		if (ImGui::InputDouble("X##pos", &pos[0], 0.0, 0.0, "%.6g")) dirty = true;
		if (ImGui::InputDouble("Y##pos", &pos[1], 0.0, 0.0, "%.6g")) dirty = true;
		if (ImGui::InputDouble("Z##pos", &pos[2], 0.0, 0.0, "%.6g")) dirty = true;
		ImGui::SeparatorText("Velocity [m/s]");
		if (ImGui::InputDouble("X##vel", &vel[0], 0.0, 0.0, "%.6g")) dirty = true;
		if (ImGui::InputDouble("Y##vel", &vel[1], 0.0, 0.0, "%.6g")) dirty = true;
		if (ImGui::InputDouble("Z##vel", &vel[2], 0.0, 0.0, "%.6g")) dirty = true;

		ImGui::Spacing();
		if (ImGui::Button("Apply state vector") && dirty) {
			vs.rpos = _V(pos[0], pos[1], pos[2]);
			vs.rvel = _V(vel[0], vel[1], vel[2]);
			v->DefSetStateEx(&vs);
		}
		ImGui::SameLine();
		if (ImGui::Button("Refresh"))
			(void)0; // reread on next frame
	}

	// ----- Orientation tab -----
	void DrawOrientationTab(VESSEL *v) {
		VECTOR3 arot;
		v->GetGlobalOrientation(arot);
		double euler[3] = { arot.x * DEG, arot.y * DEG, arot.z * DEG };
		bool dirty = false;
		ImGui::SeparatorText("Euler angles [deg]");
		if (ImGui::InputDouble("Pitch (X)", &euler[0], 0.0, 0.0, "%.4f")) dirty = true;
		if (ImGui::InputDouble("Yaw   (Y)", &euler[1], 0.0, 0.0, "%.4f")) dirty = true;
		if (ImGui::InputDouble("Roll  (Z)", &euler[2], 0.0, 0.0, "%.4f")) dirty = true;
		if (ImGui::Button("Apply orientation") && dirty) {
			VESSELSTATUS2 vs;
			std::memset(&vs, 0, sizeof(vs));
			vs.version = 2;
			v->GetStatusEx(&vs);
			vs.arot = _V(euler[0]*RAD, euler[1]*RAD, euler[2]*RAD);
			v->DefSetStateEx(&vs);
		}
	}

	// ----- Angular velocity tab -----
	void DrawAngVelTab(VESSEL *v) {
		VECTOR3 vrot;
		v->GetAngularVel(vrot);
		double w[3] = { vrot.x * DEG, vrot.y * DEG, vrot.z * DEG };
		bool dirty = false;
		ImGui::SeparatorText("Angular velocity [deg/s]");
		if (ImGui::InputDouble("Pitch rate (X)", &w[0], 0.0, 0.0, "%.4f")) dirty = true;
		if (ImGui::InputDouble("Yaw rate   (Y)", &w[1], 0.0, 0.0, "%.4f")) dirty = true;
		if (ImGui::InputDouble("Roll rate  (Z)", &w[2], 0.0, 0.0, "%.4f")) dirty = true;
		if (ImGui::Button("Apply angular velocity") && dirty) {
			v->SetAngularVel(_V(w[0]*RAD, w[1]*RAD, w[2]*RAD));
		}
		if (ImGui::Button("Stop rotation"))
			v->SetAngularVel(_V(0, 0, 0));
	}

	// ----- Propellant tab -----
	void DrawPropellantTab(VESSEL *v) {
		DWORD n = v->GetPropellantCount();
		if (n == 0) {
			ImGui::TextDisabled("Vessel has no propellant resources.");
			return;
		}
		for (DWORD i = 0; i < n; ++i) {
			PROPELLANT_HANDLE ph = v->GetPropellantHandleByIndex(i);
			double mass = v->GetPropellantMass(ph);
			double max  = v->GetPropellantMaxMass(ph);
			float frac = (max > 0) ? (float)(mass / max) : 0.f;
			ImGui::PushID((int)i);
			ImGui::Text("Tank %u: %.0f / %.0f kg", (unsigned)i, mass, max);
			if (ImGui::SliderFloat("##fill", &frac, 0.0f, 1.0f, "fill %.2f")) {
				v->SetPropellantMass(ph, frac * max);
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Empty")) v->SetPropellantMass(ph, 0.0);
			ImGui::SameLine();
			if (ImGui::SmallButton("Full"))  v->SetPropellantMass(ph, max);
			ImGui::PopID();
		}
	}

	// ----- Date tab -----
	void DrawDateTab() {
		double mjd = oapiGetSimMJD();
		double newMjd = mjd;
		ImGui::SeparatorText("Modified Julian Date");
		ImGui::InputDouble("MJD", &newMjd, 0.0, 0.0, "%.6f");
		if (ImGui::Button("Apply") && std::fabs(newMjd - mjd) > 1e-10)
			oapiSetSimMJD(newMjd, PROP_ORBITAL_FIXEDSURF);
		ImGui::SameLine();
		if (ImGui::Button("+1 hour"))   oapiSetSimMJD(mjd + 1.0/24.0, PROP_ORBITAL_FIXEDSURF);
		ImGui::SameLine();
		if (ImGui::Button("+1 day"))    oapiSetSimMJD(mjd + 1.0,      PROP_ORBITAL_FIXEDSURF);
	}
};

ScnEditorImGui *g_imguiEditor = nullptr;

} // namespace

// Entry points called from ScnEditor::OpenDialog/CloseDialog when
// !defined(_WIN32). The Win32 build keeps using the original
// resource-driven dialog flow.
void ScnEditorImGui_Open()
{
	if (!g_imguiEditor) g_imguiEditor = new ScnEditorImGui();
	g_imguiEditor->Activate();
}

void ScnEditorImGui_Close()
{
	if (g_imguiEditor) {
		// Closing equals deactivation so the dialog manager stops
		// rendering it; instance lives on across re-opens.
		// (The DialogManager destructor will free it on shutdown.)
	}
}

#endif // !_WIN32
