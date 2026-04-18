// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#define STRICT 1
#define ORBITER_MODULE
#include "orbitersdk.h"
#ifndef _WIN32
#include "resource_stub.h"
#endif
#include "AC_resource.h"
#include <stdio.h>
#ifdef _WIN32
#include <io.h>
#else
#  include <unistd.h>
#  include "imgui.h"
#  define _access access
#endif

class VesselConfig;
class AtlantisConfig;

static const char *tex_hires_enabled = "Textures2//Atlantis";
static const char *tex_hires_disabled = "Textures2//~Atlantis";

static const char *vcmsh_fname = "Meshes//Atlantis//AtlantisVC.msh";
static const char *msh_hires_bkup = "Meshes//Atlantis//~AtlantisVC_hi.msh";
static const char *msh_lores_bkup = "Meshes//Atlantis//~AtlantisVC_lo.msh";

struct {
	HINSTANCE hInst;
	AtlantisConfig *item;
} gParams;

class AtlantisConfig: public LaunchpadItem {
public:
	AtlantisConfig(): LaunchpadItem() {}
	char *Name() { return (char*)"Atlantis Configuration"; }
	char *Description();
	bool clbkOpen (HWND hLaunchpad);
#ifndef _WIN32
	bool clbkRender() override;        // ImGui editor for cross-platform Launchpad
	void EnsureSeeded();
#endif
	bool TexHiresEnabled() const;
	void TexEnableHires (bool enable);
	bool MshHiresEnabled() const;
	bool MshEnableHires (bool enable);
	void InitDialog (HWND hWnd);
	void Apply (HWND hWnd);
	static INT_PTR CALLBACK DlgProc (HWND, UINT, WPARAM, LPARAM);

private:
#ifndef _WIN32
	bool m_uiSeeded   = false;         // initial state copied from disk?
	bool m_uiTexHires = false;
	bool m_uiMshHires = false;
#endif
};

char *AtlantisConfig::Description()
{
	return (char*)"Global configuration for the default Space Shuttle Atlantis.";
}

bool AtlantisConfig::clbkOpen (HWND hLaunchpad)
{
	// respond to user double-clicking the item in the list
	return OpenDialog (gParams.hInst, hLaunchpad, IDD_ACONFIG, DlgProc);
}

bool AtlantisConfig::TexHiresEnabled () const
{
	// check if the Atlantis highres texture directory is present
	return (_access (tex_hires_enabled, 0) != -1);
}

void AtlantisConfig::TexEnableHires (bool enable)
{
	if (TexHiresEnabled() == enable) return; // nothing to do

	if (enable) {
		rename (tex_hires_disabled, tex_hires_enabled);
	} else {
		// to disable the highres textures, we simply rename the directory
		// so that orbiter's texture manager can't find it
		rename (tex_hires_enabled, tex_hires_disabled);
	}
}

bool AtlantisConfig::MshHiresEnabled () const
{
	// check if backup of low-res mesh is present
	return (_access (msh_lores_bkup, 0) != -1);
}

bool AtlantisConfig::MshEnableHires (bool enable)
{
	if (MshHiresEnabled() == enable) return false; // nothing to do

	if (enable) {
		if (_access (msh_hires_bkup, 0) == -1) return false; // high-res backup not found
		if (_access (vcmsh_fname, 0) != -1 && _access (msh_lores_bkup, 0) == -1)
			rename (vcmsh_fname, msh_lores_bkup); // back up low-res mesh
		rename (msh_hires_bkup, vcmsh_fname); // activate high-res mesh
	} else {
		if (_access (msh_lores_bkup, 0) == -1) return false; // low-res backup not found
		if (_access(vcmsh_fname, 0) != -1 && _access (msh_hires_bkup, 0) == -1)
			rename (vcmsh_fname, msh_hires_bkup); // back up high-res mesh
		rename (msh_lores_bkup, vcmsh_fname); // activate low-res mesh
	}
	return true;
}

void AtlantisConfig::InitDialog (HWND hWnd)
{
	bool texhires = TexHiresEnabled();
	bool mshhires = MshHiresEnabled();
	SendDlgItemMessage (hWnd, IDC_RADIO1, BM_SETCHECK, texhires?BST_CHECKED:BST_UNCHECKED, 0);
	SendDlgItemMessage (hWnd, IDC_RADIO2, BM_SETCHECK, texhires?BST_UNCHECKED:BST_CHECKED, 0);
	SendDlgItemMessage (hWnd, IDC_RADIO3, BM_SETCHECK, mshhires?BST_CHECKED:BST_UNCHECKED, 0);
	SendDlgItemMessage (hWnd, IDC_RADIO4, BM_SETCHECK, mshhires?BST_UNCHECKED:BST_CHECKED, 0);
}

void AtlantisConfig::Apply (HWND hWnd)
{
	bool texhires = (SendDlgItemMessage (hWnd, IDC_RADIO1, BM_GETCHECK, 0, 0) == BST_CHECKED);
	TexEnableHires (texhires);
	bool mshhires = (SendDlgItemMessage (hWnd, IDC_RADIO3, BM_GETCHECK, 0, 0) == BST_CHECKED);
	MshEnableHires (mshhires);
}

INT_PTR CALLBACK AtlantisConfig::DlgProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_INITDIALOG:
		((AtlantisConfig*)lParam)->InitDialog (hWnd);
		break;
	case WM_COMMAND:
		switch (LOWORD (wParam)) {
		case IDOK:
			((AtlantisConfig*)GetWindowLongPtr (hWnd, DWLP_USER))->Apply (hWnd);
			EndDialog (hWnd, 0);
			return 0;
		case IDCANCEL:
			EndDialog (hWnd, 0);
			return 0;
		}
		break;
	}
	return 0;
}

#ifndef _WIN32
void AtlantisConfig::EnsureSeeded()
{
	if (m_uiSeeded) return;
	m_uiTexHires = TexHiresEnabled();
	m_uiMshHires = MshHiresEnabled();
	m_uiSeeded   = true;
}

// Cross-platform editor for the Orbiter macOS / Linux Launchpad.
// Drawn inside the modal popup opened from the Extra tab — the host
// supplies the surrounding ImGui::BeginPopupModal scope, so we only
// emit the form body and an Apply button. Returns true to keep the
// editor open across frames.
bool AtlantisConfig::clbkRender()
{
	EnsureSeeded();

	ImGui::TextWrapped("Activate the high-resolution texture and mesh "
		"variants of the default Space Shuttle Atlantis. The textures "
		"and the high-res VC mesh are heavier to load — keep the "
		"low-res variant on lower-spec hosts.");

	ImGui::Spacing();
	ImGui::SeparatorText("Cockpit textures");
	ImGui::RadioButton("High resolution (256/1024 pixel)", m_uiTexHires == true);
	if (ImGui::IsItemClicked()) m_uiTexHires = true;
	ImGui::RadioButton("Standard resolution",              m_uiTexHires == false);
	if (ImGui::IsItemClicked()) m_uiTexHires = false;

	ImGui::Spacing();
	ImGui::SeparatorText("Virtual cockpit mesh");
	ImGui::RadioButton("High detail",   m_uiMshHires == true);
	if (ImGui::IsItemClicked()) m_uiMshHires = true;
	ImGui::RadioButton("Standard detail", m_uiMshHires == false);
	if (ImGui::IsItemClicked()) m_uiMshHires = false;

	ImGui::Spacing();
	if (ImGui::Button("Apply")) {
		TexEnableHires(m_uiTexHires);
		MshEnableHires(m_uiMshHires);
		m_uiSeeded = false; // re-read from disk in case rename failed
	}
	return true; // host's "Close" button signals dismissal
}
#endif // !_WIN32

// ==============================================================
// The DLL entry point
// ==============================================================

DLLCLBK void InitModule (HINSTANCE hDLL)
{
	gParams.hInst = hDLL;
	gParams.item = new AtlantisConfig;
	// create the new config item
	LAUNCHPADITEM_HANDLE root = oapiFindLaunchpadItem ("Vessel configuration");
	// find the config root entry provided by orbiter
	oapiRegisterLaunchpadItem (gParams.item, root);
	// register the DG config entry
}

// ==============================================================
// The DLL exit point
// ==============================================================

DLLCLBK void ExitModule (HINSTANCE hDLL)
{
	// Unregister the launchpad items
	oapiUnregisterLaunchpadItem (gParams.item);
	delete gParams.item;
}